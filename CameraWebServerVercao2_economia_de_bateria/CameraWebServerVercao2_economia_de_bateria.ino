// --- BIBLIOTECAS ---
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_timer.h>
#include <img_converters.h>
#include <Arduino.h>
#include "fb_gfx.h"
#include "camera_index.h"
#include "esp_http_server.h"
#include "time.h" // Para Sincronização de Relógio

// --- CREDENCIAIS DE WI-FI ---
const char* ssid = "...........";
const char* password = "............";

// --- CREDENCIAIS DO TELEGRAM ---
const char* botToken = ".........";
const char* chatID = "................";

// --- FIX (NTP): Configurações do Servidor de Tempo (Relógio) ---
const char* ntpServer = "pool.ntp.org";
// Fuso horário GMT-3 (Recife/Brasil) = -3 * 3600 segundos
const long  gmtOffset_sec = -10800; 
const int   daylightOffset_sec = 0; // Sem horário de verão


// --- MELHORIA 4: Certificado Raiz do Telegram ---
// (Substitua todo o seu bloco de certificado por este)
const char* telegram_ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrG804kFP50BNjANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaMEwxCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJjAkBgNVBAMTHURpZ2lDZXJ0IFNIQTIg\n" \
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"ANhC2VGYJevpC4HvnLpHbmZ/5sU3kX/v7gVRGgrYASwg+o6cwnYcqusqNLIePpdL\n" \
"x2+sgkRjKYhR9xW53dO0idcfItTBYjTnwjZfGz9NStoiN8O8jprxM6dE8DcsSVYn\n" \
"fnKy/Ea9S8yL2rWhZcAsrS/pA8/y9pAjaS2Z68oYm/n5YunsMcD2M/L/MhaO9/bY\n" \
"NxVMwT8t0Ewpll+4Wv1vsrcRNtEWWiOtcKcFoNqVfbycfH91sP3H3vY+c3vAEO8c\n" \
"Zf4sbnk2eD0bWAeIq/9b8bHnN3gGOTs8GBlfvnYg3d6gUX+cM2PTE4c6fcuJmS0P\n" \
"S+A43vdc9NQqEMpBkn2V2kH/jLEjA10hNUN7p9LKG/2r8Gf3sZANMuUYpEefJ6iQnEPI\n" \
"tFqAd6n3CnZV2mueAPuZXmXBXGgNIJJ+rGCdGj9a+0a2yECm+2m3f0gPnK4wYAx3\n" \
"AgMBAAGjggGEMIIBfTAfBgNVHSMEGDAWgBQD3lA1VtHYbxM54Ycsf1e8+TjbrjAd\n" \
"BgNVHQ4EFgQUo2/Q3fRGHGZlIZs8EaLdPJp8d0gwDgYDVR0PAQH/BAQDAgGGMB0G\n" \
"A1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEA\n" \
"MHYGCCsGAQUFBwEBBGowaDAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNl\n" \
"cnQuY29tMEAGCCsGAQUFBzAChjRodHRwOi8vY2FjZXJ0cy5kaWdpY2VydC5jb20v\n" \
"RGlnaUNlcnRHbG9iYWxSb290Q0EuY3J0MHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6\n" \
"Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1\n" \
"oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD\n" \
"QS5jcmwwDQYJKoZIhvcNAQELBQADggEBAIBKVlPrvK3JIVP9E5yIdSYYJmAz2fFn\n" \
"pYApl8R13nRDgS+QvoKGlm8DPDbAFLpMWT4s6yI/bTwQFn3NnFwY0kRqGgJ6iYfl\n" \
"n8T/gPA8gS2u4k9fvgBYuODt1sYkLScs/Y4xONd9aT/0ozGWu/95sDZwKVlKqH0+\n" \
"D8wznSKeQDRjB6UHNfSFnNqH/NLDfA0RTOgV/kEy/Y3Tf4S/RscqgTGMiRE0jT/D\n" \
"cWNhbMhC/Yy+j3GjG6LETaUj99cBY+PjT/3Maq0Gv/GFv2hS4v/0nL6c/1t/IAR/\n" \
"eLPaaF+8yHquP+yXG/kCjR4wWaBv5pRTs5vFBl8UfjsG/L3pLz/8WvD0k3i4rNbk\n" \
"DmqdEZuC8S/f3A0/pNmBNBD0yjf2D1iPGmKfCaY" \
"-----END CERTIFICATE-----\n";


WiFiClientSecure clientTCP;

// --- SENSOR PIR ---
#define PIR_PIN 13 // --- FIX: Trocado de 3 para 13 para evitar conflito ---
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 15000; // 15 segundos

// --- MELHORIA 1: Pino do LED/Flash ---
#define FLASH_LED_PIN 4

// --- Streaming support ---
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
void startCameraServer(); 

// --- MELHORIA 5: Função de Reconexão Wi-Fi ---
unsigned long lastWifiCheck = 0;
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  
  unsigned long currentMillis = millis();
  // Tenta reconectar apenas a cada 30 segundos
  if (currentMillis - lastWifiCheck > 30000) {
    lastWifiCheck = currentMillis;
    
    Serial.println("WiFi Desconectado! Tentando reconectar...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

// --- FUNÇÃO DE ENVIO PARA O TELEGRAM (COM MELHORIAS 2, 3, 4) ---
void sendPhotoTelegram(camera_fb_t * fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi, não é possível enviar foto.");
    return;
  }

  clientTCP.stop(); // Limpa conexões anteriores

  // --- MELHORIA 4: Usar o Certificado Raiz ---
  clientTCP.setCACert(telegram_ca_cert);

  // --- MELHORIA 3: Retentativa de Conexão ---
  int retries = 3;
  while (!clientTCP.connect("api.telegram.org", 443) && retries > 0) {
    Serial.println("Falha na conexão com Telegram, tentando novamente...");
    delay(1000); // Espera 1 segundo
    retries--;
  }

  if (!clientTCP.connected()) {
    Serial.println("Erro: Impossível conectar ao Telegram após 3 tentativas.");
    return;
  }
  
  // Construção da requisição
  String boundary = "ESP32CAMBOUNDARY";
  String startRequest = "--" + boundary + "\r\n";
  startRequest += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  startRequest += String(chatID) + "\r\n--" + boundary + "\r\n";
  startRequest += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
  startRequest += "⚠️ Motion Detected!\r\n--" + boundary + "\r\n";
  startRequest += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
  startRequest += "Content-Type: image/jpeg\r\n\r\n";
  String endRequest = "\r\n--" + boundary + "--\r\n";

  int contentLength = startRequest.length() + fb->len + endRequest.length();

  String headers = "POST /bot" + String(botToken) + "/sendPhoto HTTP/1.1\r\n";
  headers += "Host: api.telegram.org\r\n";
  headers += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  headers += "Content-Length: " + String(contentLength) + "\r\n\r\n";

  // Envio dos dados
  clientTCP.print(headers);
  clientTCP.print(startRequest);
  clientTCP.write(fb->buf, fb->len);
  clientTCP.print(endRequest);


  // --- MELHORIA 2: Verificar Resposta do Server ---
  bool success = false;
  long startTime = millis();
  
  // Aguarda a resposta por até 5 segundos (timeout)
  while (clientTCP.connected() && millis() - startTime < 5000) {
    String line = clientTCP.readStringUntil('\n');
    if (line.startsWith("HTTP/1.1 200 OK")) {
      success = true; // Servidor confirmou o recebimento!
    }
    if (line == "\r") {
      break; // Fim dos cabeçalhos da resposta
    }
  }

  if (success) {
    Serial.println("📸 Foto enviada com SUCESSO ao Telegram");
  } else {
    Serial.println("⚠️ Falha no envio ao Telegram (ou resposta não recebida)");
  }
  
  clientTCP.stop();
}


// =========================================================================
// --- CONFIGURAÇÃO INICIAL (SETUP) ---
// (VERSÃO "DOWNGRADE" PARA ECONOMIZAR ENERGIA)
// =========================================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT); // Usando o pino 13

  // --- MELHORIA 1: Configurar pino do LED ---
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW); 

  // --- MELHORIA 5: Conexão Wi-Fi ---
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  // --- DOWNGRADE 1: Reduz a potência de transmissão do Wi-Fi ---
  // Isso diminui o alcance, mas economiza MUITA energia.
  WiFi.setTxPower(WIFI_POWER_5dBm);
  
  Serial.println("Iniciando conexão WiFi...");

  // Configuração da Câmera (movida para ANTES da conexão)
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
  
  // --- DOWNGRADE 2: Reduz a frequência da câmera ---
  config.xclk_freq_hz = 10000000; // Original era 20000000
  
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // Aguarda a primeira conexão Wi-Fi
  Serial.println("Aguardando conexão WiFi para mostrar o IP...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("Stream Link: http://");
  Serial.println(WiFi.localIP());

  // --- DOWNGRADE 3: Pausa para a energia estabilizar ---
  delay(1000); // Pausa de 1 segundo

  // --- FIX (NTP): Sincronizar o Relógio após conectar ao WiFi ---
  Serial.println("Sincronizando o relógio com o servidor NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Erro: Falha ao obter a hora do NTP.");
  } else {
    Serial.println("Relógio sincronizado!");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S"); 
  }

  // --- DOWNGRADE 4: Mais uma pausa antes do pico da Câmera ---
  delay(1000); // Pausa de 1 segundo

  // Inicializa a câmera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  Serial.println("Camera Ready!");

  // Inicia o servidor de streaming
  startCameraServer();
}

// --- LOOP PRINCIPAL ---
void loop() {
  // --- MELHORIA 5: Lida com a reconexão do Wi-Fi ---
  ensureWiFiConnected();

  // Verifica o sensor PIR (agora no pino 13)
  if (digitalRead(PIR_PIN) == HIGH && millis() - lastMotionTime > motionCooldown) {
    lastMotionTime = millis();
    Serial.println("🚨 Motion detected!");

    // --- MELHORIA 1: Ligar LED ---
    digitalWrite(FLASH_LED_PIN, HIGH);

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      digitalWrite(FLASH_LED_PIN, LOW); // Desliga o LED mesmo se falhar
      return;
    }
    
    // Envia a foto apenas se o WiFi estiver OK
    if (WiFi.status() == WL_CONNECTED) {
        sendPhotoTelegram(fb);
    } else {
        Serial.println("Movimento detectado, mas sem WiFi. Foto não enviada.");
    }
    
    esp_camera_fb_return(fb);

    // --- MELHORIA 1: Desligar LED ---
    digitalWrite(FLASH_LED_PIN, LOW);
  }
}