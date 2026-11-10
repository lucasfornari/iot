#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "credenciais.h"

// ---------- Configurações de Hardware ----------
#define SENSOR_PIN          13
#define SENSOR_TIPO         DHT11
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define OLED_LARGURA        128
#define OLED_ALTURA         64
#define OLED_ENDERECO       0x3C

// ---------- Configurações de Rede e MQTT ----------
#define REDE_AP_NOME        "ESP32-Sensor"
#define REDE_AP_SENHA       "12345678"
#define TOPICO_TEMPERATURA  "sensor/temperatura"
#define TOPICO_UMIDADE      "sensor/umidade"
const unsigned long INTERVALO_LEITURA_MS = 1000;

// ---------- Instâncias de Objetos ----------
DHT dht(SENSOR_PIN, SENSOR_TIPO);
Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, -1);
WebServer servidor(80);
WiFiServer mqttTcp(1883);
WiFiServer mqttWsBase(8080);
PicoWebsocket::Server<::WiFiServer> mqttWs(mqttWsBase, "mqtt");
PicoMQTT::Server brokerMqtt(mqttTcp, mqttWs);

unsigned long ultimaLeitura = 0;

void atualizarDisplay(float temp, float umid) {
  display.clearDisplay();

  // Cabeçalho
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 3);
  display.print("CLIMA INTERNO");
  display.drawLine(0, 13, 127, 13, SSD1306_WHITE);

  // Temperatura
  display.setCursor(4, 24);
  display.print("Temp:");
  display.setTextSize(2);
  display.setCursor(44, 20);
  display.printf("%.1f C", temp);

  // Divisória horizontal
  display.drawLine(4, 38, 123, 38, SSD1306_WHITE);

  // Umidade
  display.setTextSize(1);
  display.setCursor(4, 47);
  display.print("Umid:");
  display.setTextSize(2);
  display.setCursor(44, 43);
  display.printf("%.0f %%", umid);

  display.display();
}

void conectarRede() {
  if (strlen(WIFI_NOME) > 0) {
    Serial.printf("Conectando ao Wi-Fi '%s'", WIFI_NOME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Conectado! IP: http://%s\n", WiFi.localIP().toString().c_str());
      return;
    }
    Serial.println("Falha na conexão Wi-Fi.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(REDE_AP_NOME, REDE_AP_SENHA);
  Serial.printf("Ponto de acesso criado: '%s' (IP: http://%s)\n", REDE_AP_NOME, WiFi.softAPIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);

  // Inicializa I2C e Display OLED
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 28);
    display.print("Iniciando...");
    display.display();
  } else {
    Serial.println("Aviso: Falha ao inicializar display OLED.");
  }

  dht.begin();

  if (!LittleFS.begin()) {
    Serial.println("Erro: LittleFS não montado. Execute 'pio run -t uploadfs'.");
  }

  conectarRede();

  // Servidor web estático (LittleFS)
  servidor.serveStatic("/", LittleFS, "/index.html");
  servidor.serveStatic("/mqtt.min.js", LittleFS, "/mqtt.min.js");
  servidor.begin();

  brokerMqtt.begin();
  Serial.println("Sistema pronto: Web :80 | MQTT :1883 | WebSocket :8080");
}

void loop() {
  servidor.handleClient();
  brokerMqtt.loop();

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();
    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("Falha na leitura do sensor DHT11.");
      return;
    }

    Serial.printf("Temperatura: %.1f C | Umidade: %.0f %%\n", temperatura, umidade);
    brokerMqtt.publish(TOPICO_TEMPERATURA, String(temperatura, 1));
    brokerMqtt.publish(TOPICO_UMIDADE, String(umidade, 0));
    atualizarDisplay(temperatura, umidade);
  }
}
