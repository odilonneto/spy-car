#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "Arduino.h"
#include "esp_wpa2.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>

const char* ssid = "UTFPR-ALUNO";
const char* username = "a2417073";
const char* password = "gagaue34";
#define API_KEY         "AIzaSyCVocLuveyOMjhW4D77gLQTYVPteVylAXQ"
#define USER_EMAIL      "odilonspycar@gmail.com"
#define USER_PASSWORD   "12345678"
#define RTDB_URL        "https://spy-car-c29ac-default-rtdb.firebaseio.com"
const char* upload_url = "https://spycar.onrender.com/upload";

#define LED_PIN 4
#define SERVO_1 14
#define SERVO_2 15

#define PWM_MOTr 12
#define PWM_MOTl 13
#define POS_MOT 2
#define NEG_MOT 3

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

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;
FirebaseData streamCmd;

Servo servo1;
Servo servo2;

WiFiClientSecure client;
HTTPClient http;

int servo1Pos = 90;
int servo2Pos = 45;
int esquerdo = 255;
int direito = 243;

void streamCallback(FirebaseStream data) {
  String path = data.dataPath();
  String raw = data.stringData();

  String cmd = raw;
  String Rc = "";
  int idx = raw.indexOf(':');
  if (idx != -1) {
    cmd = raw.substring(0, idx);
    Rc = raw.substring(idx + 1);
  }

  if (path == "/led") {
    digitalWrite(LED_PIN, (cmd == "off") ? LOW : HIGH);
  }

  if (path == "/direito") {
    direito = raw.toInt();
  }

  if (path == "/esquerdo") {
    esquerdo = raw.toInt();
  }

  if (path == "/servo") {
    if (cmd == "up") {
      if (servo1Pos <= 200) {
        servo1Pos += 20;
        servo1.write(servo1Pos);
      }
    }
    else if (cmd == "left") {
      if(servo2Pos <= 160) {
        servo2Pos += 20;
        servo2.write(servo2Pos);
      }
    }
    else if (cmd == "right") {
      if(servo2Pos >= 20) {
        servo2Pos -= 20;
        servo2.write(servo2Pos);
      }
    }
    else if (cmd == "down") {
      if(servo1Pos >= 20) {
        servo1Pos -= 20;
        servo1.write(servo1Pos);
      }
    }
  }

  if (path == "/motor"){
    if (cmd == "stop") {
      digitalWrite(POS_MOT, LOW);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, 0);
      ledcWrite(PWM_MOTl, 0);
    }
    else if (cmd == "forward") {
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
    else if (cmd == "backward") {
      digitalWrite(POS_MOT, LOW);
      digitalWrite(NEG_MOT, HIGH);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
    else if (cmd == "left") {
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, direito * Rc.toFloat());
      ledcWrite(PWM_MOTl, 200);
    }
    else if (cmd == "right") {
      digitalWrite(POS_MOT, HIGH);
      digitalWrite(NEG_MOT, LOW);
      ledcWrite(PWM_MOTr, 180);
      ledcWrite(PWM_MOTl, esquerdo * Rc.toFloat());
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Firebase.RTDB.beginStream(&streamCmd, "/comandos");
  }
}

void initWiFi() {
  if(username!="/"){
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
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desabilita detector de brownout
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  //led
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  //servos
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);    
  servo1.attach(SERVO_1, 1000, 2000);
  servo2.attach(SERVO_2, 1000, 2000);
  servo1.write(servo1Pos);
  servo2.write(servo2Pos);
  //motores
  pinMode(POS_MOT, OUTPUT);
  pinMode(NEG_MOT, OUTPUT);
  digitalWrite(POS_MOT, LOW);
  digitalWrite(NEG_MOT, LOW);
  ledcAttach(PWM_MOTr, 5000, 8);
  ledcAttach(PWM_MOTl, 5000, 8);
  ledcWrite(PWM_MOTl, 0);
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
  config.pixel_format = PIXFORMAT_JPEG;
  // Qualidade e tamanho da imagem
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  // Inicializa a câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return;
  }

  configF.api_key = API_KEY;
  auth.user.email    = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.database_url        = RTDB_URL;
  configF.token_status_callback = tokenStatusCallback;

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.RTDB.beginStream(&streamCmd, "/comandos")) {
  }
  Firebase.RTDB.setStreamCallback(&streamCmd, streamCallback, streamTimeoutCallback);
  
  client.setInsecure();
  http.begin(client, upload_url);
  http.addHeader("Content-Type", "image/jpeg");
  http.setReuse(true);
}

void loop() {

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    return;
  }

  if (fb->format == PIXFORMAT_JPEG) {
    int httpResponseCode = http.POST(fb->buf, fb->len);
  }

  esp_camera_fb_return(fb);
}