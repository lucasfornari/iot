#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "credenciais.h"

// ---------- Sensor ----------
#define SENSOR_TEMPERATURA_UMIDADE_PIN 13
#define SENSOR_TEMPERATURA_UMIDADE_TIPO DHT11
const unsigned long INTERVALO_LEITURA_MS = 2000; // 2 segundos

// ---------- Tópicos MQTT ----------
#define TOPICO_TEMPERATURA "sensor/temperatura"
#define TOPICO_UMIDADE     "sensor/umidade"

// ---------- Rede própria (usada se não conectar no Wi-Fi) ----------
#define REDE_PROPRIA_NOME  "ESP32-Sensor"
#define REDE_PROPRIA_SENHA "12345678"

// ---------- Portas ----------
// 80   -> página web (index.html)
// 1883 -> broker MQTT (clientes comuns: mosquitto_sub, MQTT Explorer...)
// 8080 -> broker MQTT via WebSocket (usado pelo navegador)
WebServer servidorPagina(80);
WiFiServer servidorMqttTcp(1883);
WiFiServer servidorMqttWebsocketBase(8080);
PicoWebsocket::Server<::WiFiServer> servidorMqttWebsocket(servidorMqttWebsocketBase, "mqtt");
PicoMQTT::Server brokerMqtt(servidorMqttTcp, servidorMqttWebsocket);

DHT sensorTemperaturaUmidade(SENSOR_TEMPERATURA_UMIDADE_PIN, SENSOR_TEMPERATURA_UMIDADE_TIPO);
unsigned long ultimaLeitura = 0;

void conectarRede()
{
  if (strlen(WIFI_NOME) > 0)
  {
    Serial.printf("Conectando no Wi-Fi '%s'", WIFI_NOME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
    for (int tentativa = 0; tentativa < 30 && WiFi.status() != WL_CONNECTED; tentativa++)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("Conectado! Abra no navegador: http://%s\n", WiFi.localIP().toString().c_str());
      return;
    }
    Serial.println("Não conectou no Wi-Fi.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(REDE_PROPRIA_NOME, REDE_PROPRIA_SENHA);
  Serial.printf("Rede própria criada: '%s' (senha %s)\n", REDE_PROPRIA_NOME, REDE_PROPRIA_SENHA);
  Serial.printf("Conecte nela e abra: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void setup()
{
  Serial.begin(115200);
  sensorTemperaturaUmidade.begin();

  if (!LittleFS.begin())
  {
    Serial.println("Erro: LittleFS vazio. Rode 'pio run -t uploadfs'.");
  }

  conectarRede();

  servidorPagina.serveStatic("/", LittleFS, "/index.html");
  servidorPagina.serveStatic("/mqtt.min.js", LittleFS, "/mqtt.min.js");
  servidorPagina.begin();
  brokerMqtt.begin();

  Serial.println("Página na porta 80 | MQTT na 1883 | MQTT/WebSocket na 8080");
}

void loop()
{
  servidorPagina.handleClient();
  brokerMqtt.loop();

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS)
  {
    ultimaLeitura = millis();
    float temperatura = sensorTemperaturaUmidade.readTemperature();
    float umidade = sensorTemperaturaUmidade.readHumidity();

    if (isnan(temperatura) || isnan(umidade))
    {
      Serial.println("Falha na leitura do sensor de temperatura e umidade");
      return;
    }

    Serial.printf("Temperatura: %.1f C | Umidade: %.0f %%\n", temperatura, umidade);
    brokerMqtt.publish(TOPICO_TEMPERATURA, String(temperatura, 1));
    brokerMqtt.publish(TOPICO_UMIDADE, String(umidade, 0));
  }
}
