/*
 * =====================================================================================
 * VERSÕES DAS BIBLIOTECAS UTILIZADAS (verificado em 2025)
 * =====================================================================================
 *
 * - ESP32Servo por Kevin Harrington, John K. Bennett
 * Versão: 3.0.6
 *
 * - Firebase Arduino Client Library for ESP8266 and ESP32 por Mobizt
 * Versão: 4.4.17
 *
 * - Firebase ESP32 Client por Mobizt
 * Versão: 4.4.17
 *
 * =====================================================================================
 * VERSÕES DO GERENCIADOR DE PLACAS (verificado em 2025)
 * =====================================================================================
 *
 * - Arduino AVR Boards por Arduino
 * Versão: 1.8.6
 *
 * - esp32 por Espressif Systems
 * Versão: 3.2.0
 *
 * =====================================================================================
*/

// Bibliotecas principais do ESP32 e câmera
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "Arduino.h"

// Bibliotecas para conexão Wi-Fi WPA2 Enterprise (ex: redes de universidade)
#include "esp_wpa2.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Bibliotecas do Firebase
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <WiFiClientSecure.h>

// Biblioteca para controle de Servos
#include <ESP32Servo.h>

// Usar internet da faculdade (WPA2 Enterprise)
const char* ssid = ""; // SSID da rede
const char* username = ""; // Nome de usuário (geralmente o mesmo do login da rede)
const char* password = ""; // Senha da rede

// Usar internet de casa (WPA2 Personal)
//const char* ssid = ""; // SSID da rede
//const char* username = "/"; // Deixe "/" para indicar que não é WPA2 Enterprise
//const char* password = ""; // Senha da rede

// --- Credenciais do Firebase ---
#define API_KEY         "" // Web API Key do Firebase
#define USER_EMAIL      "" // E-mail do usuário do Firebase
#define USER_PASSWORD   "" // Senha do usuário do Firebase
#define RTDB_URL        "" // URL do Realtime Database do Firebase

// URL do servidor que receberá o stream de vídeo (via HTTP POST)
const char* upload_url = ""; // Ex: "http://seu_usuario.onrender.com/upload"

// --- Mapeamento de Pinos (GPIOs) ---
#define LED_PIN 4     // LED (flash da câmera)
#define SERVO_1 14    // Servo Vertical (Tilt)
#define SERVO_2 15    // Servo Horizontal (Pan)

#define PWM_MOTr 12   // PWM Motor Direito (Velocidade)
#define PWM_MOTl 13   // PWM Motor Esquerdo (Velocidade)
#define POS_MOT 2     // Controle de Direção 1 (IN1/IN4 da Ponte H)
#define NEG_MOT 3     // Controle de Direção 2 (IN2/IN3 da Ponte H)

// Configuração de Pinos da Câmera (Padrão AI-Thinker ESP32-CAM)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- Variáveis Globais ---

// Objetos do Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;
FirebaseData streamCmd;   // Objeto para monitorar o stream de comandos

// Objetos dos Servos
Servo servo1; 
Servo servo2;

// Clientes de Rede
WiFiClientSecure client;  // Cliente para Firebase
HTTPClient http;          // Cliente para upload de imagem

// Variáveis de estado (reset)
int servo1Pos = 90;       // Posição inicial Servo Vertical (meio)
int servo2Pos = 45;       // Posição inicial Servo Horizontal (meio)
int esquerdo = 255;       // Velocidade máxima (PWM) motor esquerdo
int direito = 243;        // Velocidade máxima (PWM) motor direito (ajuste de balanço)

// Função streamCallback é chamada toda vez que há uma mudança de dados no nó "/comandos"
void streamCallback(FirebaseStream data) {
  String path = data.dataPath();  // Caminho do dado
  String raw = data.stringData(); // Valor do dado

  // Divide o dado recebido em comando (cmd) e valor (Rc)
  String cmd = raw;
  String Rc = "";
  int idx = raw.indexOf(':');
  if (idx != -1) {
    cmd = raw.substring(0, idx);
    Rc = raw.substring(idx + 1);
  }

  // --- Lógica de Comandos ---

  // Controle do LED
  if (path == "/led") {
    digitalWrite(LED_PIN, (cmd == "off") ? LOW : HIGH);
  }

  // Ajuste de velocidade dos motores
  if (path == "/direito") {
    direito = raw.toInt();
  }

  if (path == "/esquerdo") {
    esquerdo = raw.toInt();
  }

  // Controle dos Servos
  if (path == "/servo") {
    // ATENÇÃO: A lógica 'up' e 'down' está invertida propositalmente
    // devido à montagem física servo.

    if (cmd == "up") {
      // Comando "up" deve SUBTRAIR (pois seu servo está invertido)
      // A checagem de limite precisa garantir que (posição - 20) não seja MENOR que 20.
      if (servo1Pos - 20 >= 20) { 
        servo1Pos -= 20; // Ação: Diminuir ângulo
        servo1.write(servo1Pos);
      }
    }
  else if (cmd == "left") {
    // Lógica do servo2 (horizontal) 
    if(servo2Pos + 20 <= 160) {
      servo2Pos += 20;
      servo2.write(servo2Pos);
    }
  }
  else if (cmd == "right") {
    // Lógica do servo2 (horizontal)
    if(servo2Pos - 20 >= 20) {
      servo2Pos -= 20;
      servo2.write(servo2Pos);
    }
  }
  else if (cmd == "down") {
    // Comando "down" deve SOMAR (pois seu servo está invertido)
    // A checagem de limite precisa garantir que (posição + 20) não seja MAIOR que 200.
    if(servo1Pos + 20 <= 200) { 
      servo1Pos += 20; // Ação: Aumentar ângulo
      servo1.write(servo1Pos);
    }
  }
}

  // Controle dos Motores (Ponte H)
  if (path == "/motor"){
    if (cmd == "stop") {
      // Desliga os pinos de direção e zera o PWM
      digitalWrite(POS_MOT, LOW);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, 0);
      ledcWrite(PWM_MOTl, 0);
    }
    else if (cmd == "forward") {
      // Define a direção "frente" e aplica PWM com multiplicador (Rc)
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
    else if (cmd == "backward") {
      // Define a direção "ré" e aplica PWM com multiplicador (Rc)
      digitalWrite(POS_MOT, LOW);
      digitalWrite(NEG_MOT, HIGH);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
    else if (cmd == "left") {
      // Gira para esquerda (motor direito para frente, motor esquerdo com PWM reduzido/parado)
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, 200);
    }
    else if (cmd == "right") {
      // Gira para direita (motor esquerdo para frente, motor direito com PWM reduzido/parado)
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, 180);
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
  }
}

// Função streamTimeoutCallback é chamada se o stream do Firebase cair e tenta reiniciar o stream.
void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    // Tenta reiniciar o stream no nó "/comandos"
    Firebase.RTDB.beginStream(&streamCmd, "/comandos");
  }
}

// Função initWiFi inializa a conexão Wi-fi
void initWiFi() {
  if(username!="/"){
    // --- Conexão WPA2 Enterprise (Universidade) ---
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    esp_wifi_sta_wpa2_ent_enable();
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));

    WiFi.begin(ssid);

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }

  }
  else {
    // --- Conexão WPA2 Personal (Casa) ---
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
  }
}

// Função de setup é executada uma vez na inicialização
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desabilita detector de brownout
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // --- Configuração dos Pinos de Saída ---

  //LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Servos
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);    
  servo1.attach(SERVO_1, 1000, 2000);
  servo2.attach(SERVO_2, 1000, 2000);
  servo1.write(servo1Pos);
  servo2.write(servo2Pos);
  
  // Motores (Ponte H)
  pinMode(POS_MOT, OUTPUT);
  pinMode(NEG_MOT, OUTPUT);
  digitalWrite(POS_MOT, LOW);
  digitalWrite(NEG_MOT, LOW);

  // Configura os canais PWM para os motores
  ledcAttach(PWM_MOTr, 5000, 8);  // Canal, Frequência (5kHz), Resolução (8 bits = 0-255)
  ledcAttach(PWM_MOTl, 5000, 8);
  ledcWrite(PWM_MOTl, 0);         // Inicia com velocidade 0
  ledcWrite(PWM_MOTr, 0);

  // Inicializar Wi-Fi
  initWiFi();

  // Configuração da câmera
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
  config.pixel_format = PIXFORMAT_JPEG; // Formato de saída

  // Qualidade e tamanho da imagem
  // FRAMESIZE_SVGA (800x600) oferece um bom equilíbrio entre qualidade e velocidade
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12; // 0-63 (mais baixo = melhor qualidade, mais lento)
  config.fb_count = 1;      // Número de buffers de frame

  // Inicializa a câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return;
  }

  // --- Configuração do Firebase ---
  configF.api_key = API_KEY;
  auth.user.email    = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.database_url        = RTDB_URL;
  configF.token_status_callback = tokenStatusCallback;

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true); // Tenta reconectar o WiFi se cair

  // Inicia o monitoramento (stream) do nó "/comandos"
  if (!Firebase.RTDB.beginStream(&streamCmd, "/comandos")) {
  }
  Firebase.RTDB.setStreamCallback(&streamCmd, streamCallback, streamTimeoutCallback);
  
  // --- Configuração do Cliente HTTP (Upload de Imagem) ---
  client.setInsecure(); // Permite conexões com servidores sem certificado SSL (onrender.com)
  http.begin(client, upload_url);
  http.addHeader("Content-Type", "image/jpeg");
  http.setReuse(true);  // Reutiliza a conexão TCP para mais velocidade
}

// Função loop (executado continuamente).
void loop() {

  // 1. Captura um quadro (frame) da câmera
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    return;
  }

  // 2. Verifica se o formato é JPEG e envia via HTTP POST
  if (fb->format == PIXFORMAT_JPEG) {
    int httpResponseCode = http.POST(fb->buf, fb->len);

    // Imprime o código de resposta do servidor (para debug)
    // HTTP POST Code: 200 => OK
    Serial.printf("HTTP POST Code: %d\n", httpResponseCode);
  }

  // 3. Libera o buffer do quadro da câmera
  // (MUITO IMPORTANTE! Se esquecer, a câmera trava)
  esp_camera_fb_return(fb);
}