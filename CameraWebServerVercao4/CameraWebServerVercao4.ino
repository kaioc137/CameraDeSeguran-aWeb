//SciCraft
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiManager.h>        // Gerenciador de WiFi
#include <WiFiClientSecure.h>
#include <esp_timer.h>
#include <img_converters.h>
#include <Arduino.h>
#include "fb_gfx.h"
#include "camera_index.h"
#include "esp_http_server.h"

// --- MUDANÇA 1: Bibliotecas para salvar a configuração ---
#include <LittleFS.h>
#include <ArduinoJson.h>

// --- MUDANÇA 2: Variáveis do Telegram (não mais constantes) ---
// Definimos o tamanho máximo para os campos
char botToken[64] = "";
char chatID[20] = "";

WiFiClientSecure clientTCP;

// PIR sensor
#define PIR_PIN 13 
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000; // 5 seconds

// Pino do LED do Flash
#define FLASH_LED_PIN 4

// Streaming support
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
void startCameraServer();

// --- MUDANÇA 3: Funções para Salvar e Carregar a Configuração ---
const char *configPath = "/config.json";

// Função para salvar a configuração no LittleFS
void saveConfig() {
  Serial.println("Salvando configuração...");
  DynamicJsonDocument json(256);
  json["botToken"] = botToken;
  json["chatID"] = chatID;

  File configFile = LittleFS.open(configPath, "w");
  if (!configFile) {
    Serial.println("Falha ao abrir arquivo de config para escrita");
    return;
  }

  serializeJson(json, configFile);
  configFile.close();
  Serial.println("Configuração salva.");
}

// Função para carregar a configuração do LittleFS
void loadConfig() {
  // Inicializa o LittleFS
  if (!LittleFS.begin(true)) { // true = formatar se a montagem falhar
    Serial.println("Falha ao montar LittleFS");
    return;
  }

  if (LittleFS.exists(configPath)) {
    File configFile = LittleFS.open(configPath, "r");
    if (configFile) {
      DynamicJsonDocument json(256);
      DeserializationError error = deserializeJson(json, configFile);
      
      if (error) {
        Serial.println("Falha ao ler config, usando valores padrão (vazios)");
      } else {
        Serial.println("Carregando configuração salva:");
        // Carrega os valores nos nossos arrays
        // O | "" garante que, se a chave não existir, ele preenche com vazio
        strcpy(botToken, json["botToken"] | "");
        strcpy(chatID, json["chatID"] | "");

        Serial.println("Configuração carregada.");
      }
      configFile.close();
    }
  } else {
    Serial.println("Arquivo de config não encontrado, usando valores padrão (vazios).");
  }
}

// (Função sendPhotoTelegram não precisa de NENHUMA alteração)
void sendPhotoTelegram(camera_fb_t * fb) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Se o botToken ou chatID estiverem vazios, não tenta enviar.
  if (strlen(botToken) == 0 || strlen(chatID) == 0) {
    Serial.println("Bot Token ou Chat ID não configurados. Pule o envio.");
    return;
  }

  clientTCP.stop();
  clientTCP.setInsecure(); 

  int retries = 3;
  while (!clientTCP.connect("api.telegram.org", 443) && retries > 0) {
    Serial.println("Falha na conexão com Telegram, tentando novamente...");
    delay(1000); 
    retries--;
  }
  
  if (!clientTCP.connected()) {
    Serial.println("Erro: Impossível conectar ao Telegram após 3 tentativas.");
    return;
  }
  
  String boundary = "ESP32CAMBOUNDARY";
  String startRequest = "--" + boundary + "\r\n";
  startRequest += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  startRequest += String(chatID) + "\r\n--" + boundary + "\r\n";
  startRequest += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";

  String ipAddress = "http://" + WiFi.localIP().toString() + "/";
  String caption = "⚠️ Movimento Detectado!\r\nStream: " + ipAddress;
  startRequest += caption + "\r\n--" + boundary + "\r\n";

  startRequest += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
  startRequest += "Content-Type: image/jpeg\r\n\r\n";
  String endRequest = "\r\n--" + boundary + "--\r\n";

  int contentLength = startRequest.length() + fb->len + endRequest.length();

  String headers = "POST /bot" + String(botToken) + "/sendPhoto HTTP/1.1\r\n";
  headers += "Host: api.telegram.org\r\n";
  headers += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  headers += "Content-Length: " + String(contentLength) + "\r\n\r\n";

  clientTCP.print(headers);
  clientTCP.print(startRequest);
  clientTCP.write(fb->buf, fb->len);
  clientTCP.print(endRequest);

  bool success = false;
  long startTime = millis();
  
  while (clientTCP.connected() && millis() - startTime < 5000) {
    String line = clientTCP.readStringUntil('\n');
    if (line.startsWith("HTTP/1.1 200 OK")) {
      success = true; 
    }
    if (line == "\r") break; 
  }

  if (success) {
    Serial.println("📸 Foto (com IP) enviada com SUCESSO ao Telegram");
  } else {
    Serial.println("⚠️ Falha no envio ao Telegram (ou resposta não recebida)");
  }
  
  clientTCP.stop();
}

// --- MUDANÇA 4: Função setup() modificada ---
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  // 1. CARREGA a configuração salva (se existir)
  loadConfig();
  
  // CONFIGURAÇÃO DO WIFIMANAGER
  WiFi.mode(WIFI_STA); 
  WiFiManager wm; 

  // 2. CRIA OS CAMPOS CUSTOMIZADOS no formulário do WiFiManager
  // Os valores carregados (botToken, chatID) são usados como valor "padrão"
  // Se estiverem vazios, os campos aparecem vazios. Se já tiverem sido salvos,
  // eles aparecem pré-preenchidos.
  WiFiManagerParameter custom_bot_token("botToken", "Bot Token (Telegram)", botToken, 64);
  WiFiManagerParameter custom_chat_id("chatID", "Chat ID (Telegram)", chatID, 20);

  // 3. ADICIONA os campos ao WiFiManager
  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_chat_id);

  // 4. INICIA o portal
  bool res = wm.autoConnect("ESP32-CAM-Setup"); 

  if(!res) {
      Serial.println("Falha ao configurar. Reiniciando...");
      delay(5000);
      ESP.restart(); 
  } 
  
  Serial.println("\nWiFi conectado!");

  // 5. PEGA OS VALORES e SALVA
  // Pega os valores que o usuário digitou no portal
  // (Se o portal não foi aberto, ele "pega" os valores padrão que já carregamos)
  strcpy(botToken, custom_bot_token.getValue());
  strcpy(chatID, custom_chat_id.getValue());
  
  Serial.println("Valores de Bot configurados:");
  Serial.print("Bot Token: "); Serial.println(botToken);
  Serial.print("Chat ID: "); Serial.println(chatID);

  // Salva os valores atuais no LittleFS
  saveConfig();

  // ----- FIM DAS MUDANÇAS NO SETUP -----

  // Configuração da Câmera (sem alterações)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; // Mantenha QVGA para velocidade no Telegram
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Falha na inicialização da câmera");
    return;
  }

  Serial.println("Câmera pronta!");
  startCameraServer();
  Serial.print("Stream Link: http://");
  Serial.println(WiFi.localIP()); 
}

// (Função loop não precisa de NENHUMA alteração)
void loop() {
  if (digitalRead(PIR_PIN) == HIGH && millis() - lastMotionTime > motionCooldown) {
    lastMotionTime = millis();
    Serial.println("🚨 Movimento detectado!");

    digitalWrite(FLASH_LED_PIN, HIGH); 

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      digitalWrite(FLASH_LED_PIN, LOW); 
      return;
    }
    
    sendPhotoTelegram(fb); // Esta função agora envia o IP junto
    esp_camera_fb_return(fb);
    
    digitalWrite(FLASH_LED_PIN, LOW);
  }
}
