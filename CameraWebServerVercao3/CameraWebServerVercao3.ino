//SciCraft
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiManager.h>       // Gerenciador de WiFi
#include <WiFiClientSecure.h>
#include <esp_timer.h>
#include <img_converters.h>
#include <Arduino.h>
#include "fb_gfx.h"
#include "camera_index.h"
#include "esp_http_server.h"

// Telegram bot (PREENCHA COM SEUS DADOS)
const char* botToken = "8388159305:AAHOcX8D_rV8Z8iJQ61ZlOUnn2Krp2WnsRI";
const char* chatID = "5521417949";

WiFiClientSecure clientTCP;

// PIR sensor
#define PIR_PIN 13 // Pino seguro
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000; // 5 seconds

// MELHORIA 1: Pino do LED do Flash
#define FLASH_LED_PIN 4

// Streaming support
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
void startCameraServer(); 

void sendPhotoTelegram(camera_fb_t * fb) {
  if (WiFi.status() != WL_CONNECTED) return;

  clientTCP.stop();
  clientTCP.setInsecure(); // Mantemos para economizar RAM

  // MELHORIA 3: Retentativa de Conexão
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

  // --- NOVA ADIÇÃO: Enviar o Link do Stream na Legenda ---
  // Pega o IP atual e o transforma em String
  String ipAddress = "http://" + WiFi.localIP().toString() + "/";
  String caption = "⚠️ Movimento Detectado!\r\nStream: " + ipAddress;
  startRequest += caption + "\r\n--" + boundary + "\r\n";
  // --- FIM DA NOVA ADIÇÃO ---

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

  // MELHORIA 2: Verificar a Resposta do Servidor
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

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT); // MELHORIA 1
  digitalWrite(FLASH_LED_PIN, LOW); // MELHORIA 1
  
  // CONFIGURAÇÃO DO WIFIMANAGER
  WiFi.mode(WIFI_STA); 
  WiFiManager wm; 

  bool res = wm.autoConnect("ESP32-CAM-Setup"); 

  if(!res) {
      Serial.println("Falha ao configurar. Reiniciando...");
      delay(5000);
      ESP.restart(); 
  } 
  Serial.println("\nWiFi conectado!");

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
  config.frame_size = FRAMESIZE_QVGA;
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

void loop() {
  if (digitalRead(PIR_PIN) == HIGH && millis() - lastMotionTime > motionCooldown) {
    lastMotionTime = millis();
    Serial.println("🚨 Movimento detectado!");

    digitalWrite(FLASH_LED_PIN, HIGH); // MELHORIA 1

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      digitalWrite(FLASH_LED_PIN, LOW); 
      return;
    }
    
    sendPhotoTelegram(fb); // Esta função agora envia o IP junto
    esp_camera_fb_return(fb);
    
    digitalWrite(FLASH_LED_PIN, LOW); // MELHORIA 1
  }
}