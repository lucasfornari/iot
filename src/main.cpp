// Firmware Clima Interno + Alexa (Sinric Pro)
// ESP32 + DHT11 + OLED SSD1306. Publica temperatura/umidade no Sinric Pro,
// que expõe o sensor para a Alexa. O controle por palmas (MAX4466) está
// preservado em extras/firmware_palmas/ e fora desta compilação.

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SinricPro.h>
#include <SinricProTemperaturesensor.h>
#include "credenciais.h"

// ---------- Configurações de Hardware ----------
#define PINO_SENSOR_DHT     13
#define TIPO_SENSOR_DHT     DHT11
#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22
#define LARGURA_TELA_OLED   128
#define ALTURA_TELA_OLED    64
#define ENDERECO_TELA_OLED  0x3C

// ---------- Temporização ----------
const unsigned long INTERVALO_DHT = 2000;          // DHT11 aceita 1 leitura/s; 2 s dá folga
const unsigned long INTERVALO_TELA = 500;
const unsigned long INTERVALO_MIN_EVENTO = 60000;  // Limite do Sinric para eventos de sensor
const unsigned long INTERVALO_MAX_EVENTO = 300000; // Reenvia a cada 5 min mesmo sem mudança
const unsigned long TIMEOUT_WIFI_SETUP = 15000;    // Espera inicial; depois reconecta em segundo plano

// Variação mínima para antecipar um envio (resolução do DHT11 é 1 °C / 1 %).
const float DELTA_TEMPERATURA = 0.5;
const float DELTA_UMIDADE = 2.0;

// ---------- Instâncias Globais ----------
DHT sensorDht(PINO_SENSOR_DHT, TIPO_SENSOR_DHT);
Adafruit_SSD1306 telaOled(LARGURA_TELA_OLED, ALTURA_TELA_OLED, &Wire, -1);
bool telaDisponivel = false;

float temperaturaAtual = NAN;
float umidadeAtual = NAN;
unsigned long tempoUltimoDHT = 0;

float temperaturaEnviada = NAN;
float umidadeEnviada = NAN;
unsigned long tempoUltimoEvento = 0;
bool enviarAgora = true;  // Primeiro envio assim que conectar ao Sinric

bool credenciaisSinricOk = false;
bool sinricConectado = false;

// ---------- Interface OLED bicolor ----------
// A cor é física: linhas 0–15 amarelas, linhas 16–63 azuis.
// Mantém cada texto inteiro em uma única faixa, com margem na transição.
const int ALTURA_FAIXA_AMARELA = 16;

void desenharTextoCentralizado(const char *texto, int y, int tamanho) {
  telaOled.setTextSize(tamanho);
  int16_t xInicial, yInicial;
  uint16_t largura, altura;
  telaOled.getTextBounds(texto, 0, y, &xInicial, &yInicial, &largura, &altura);
  telaOled.setCursor((LARGURA_TELA_OLED - int(largura)) / 2 - xInicial, y);
  telaOled.print(texto);
}

void desenharCabecalho() {
  telaOled.clearDisplay();
  telaOled.setTextWrap(false);
  telaOled.setTextColor(SSD1306_WHITE);
  desenharTextoCentralizado("CLIMA INTERNO", 4, 1);
  telaOled.drawFastHLine(8, ALTURA_FAIXA_AMARELA + 1, 112, SSD1306_WHITE);
}

void desenharMensagem(const char *linha1, const char *linha2) {
  if (!telaDisponivel) return;
  desenharCabecalho();
  desenharTextoCentralizado(linha1, 28, 1);
  desenharTextoCentralizado(linha2, 42, 1);
  telaOled.display();
}

const char *textoStatusWifi() {
  return WiFi.status() == WL_CONNECTED ? "OK" : "--";
}

const char *textoStatusAlexa() {
  if (!credenciaisSinricOk) return "CFG";
  return sinricConectado ? "OK" : "--";
}

void desenharPainel() {
  if (!telaDisponivel) return;
  desenharCabecalho();

  char texto[24];
  // "\xF7" é o símbolo de grau na fonte cp437 da Adafruit GFX.
  if (isnan(temperaturaAtual)) {
    snprintf(texto, sizeof(texto), "--.- " "\xF7" "C");
  } else {
    snprintf(texto, sizeof(texto), "%.1f " "\xF7" "C", temperaturaAtual);
  }
  desenharTextoCentralizado(texto, 22, 2);

  if (isnan(umidadeAtual)) {
    snprintf(texto, sizeof(texto), "Umidade: -- %%");
  } else {
    snprintf(texto, sizeof(texto), "Umidade: %.0f %%", umidadeAtual);
  }
  desenharTextoCentralizado(texto, 42, 1);

  snprintf(texto, sizeof(texto), "WiFi:%s  Alexa:%s", textoStatusWifi(), textoStatusAlexa());
  desenharTextoCentralizado(texto, 55, 1);
  telaOled.display();
}

// ---------- Wi-Fi ----------
void iniciarWifi() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("[WIFI] WIFI_SSID vazio: preencha o arquivo .env");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);         // Recomendação do Sinric: evita quedas do WebSocket
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);

  Serial.printf("[WIFI] Conectando a \"%s\"", WIFI_SSID);
  desenharMensagem("Conectando WiFi...", WIFI_SSID);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < TIMEOUT_WIFI_SETUP) {
    Serial.print(".");
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" ok! IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" sem resposta; seguindo offline e tentando em segundo plano.");
  }
}

// ---------- Sinric Pro ----------
void iniciarSinric() {
  credenciaisSinricOk = strlen(SINRIC_APP_KEY) > 0 && strlen(SINRIC_APP_SECRET) > 0 &&
                        strlen(SINRIC_DEVICE_ID) > 0;
  if (!credenciaisSinricOk) {
    Serial.println("[SINRIC] Credenciais ausentes no .env; Alexa desativada.");
    return;
  }

  // A conversão para referência é o que cria/registra o dispositivo no SDK.
  SinricProTemperaturesensor &sensor = SinricPro[SINRIC_DEVICE_ID];
  (void)sensor;
  SinricPro.onConnected([]() {
    sinricConectado = true;
    enviarAgora = true;  // Atualiza a Alexa logo após (re)conectar
    Serial.println("[SINRIC] Conectado");
  });
  SinricPro.onDisconnected([]() {
    sinricConectado = false;
    Serial.println("[SINRIC] Desconectado");
  });
  SinricPro.begin(SINRIC_APP_KEY, SINRIC_APP_SECRET);
}

void enviarLeituraSinric() {
  if (!credenciaisSinricOk || !sinricConectado) return;
  if (isnan(temperaturaAtual) || isnan(umidadeAtual)) return;

  unsigned long agora = millis();
  unsigned long decorrido = agora - tempoUltimoEvento;
  // O próprio SDK descarta eventos com menos de 60 s; não gasta a tentativa.
  if (tempoUltimoEvento != 0 && decorrido < INTERVALO_MIN_EVENTO) return;

  bool mudou = isnan(temperaturaEnviada) ||
               fabsf(temperaturaAtual - temperaturaEnviada) >= DELTA_TEMPERATURA ||
               fabsf(umidadeAtual - umidadeEnviada) >= DELTA_UMIDADE;
  if (!enviarAgora && !mudou && decorrido < INTERVALO_MAX_EVENTO) return;

  SinricProTemperaturesensor &sensor = SinricPro[SINRIC_DEVICE_ID];
  bool ok = sensor.sendTemperatureEvent(temperaturaAtual, umidadeAtual);
  tempoUltimoEvento = agora == 0 ? 1 : agora;
  enviarAgora = false;
  if (ok) {
    temperaturaEnviada = temperaturaAtual;
    umidadeEnviada = umidadeAtual;
    Serial.printf("[SINRIC] Enviado: %.1f C, %.0f %%\n", temperaturaAtual, umidadeAtual);
  } else {
    Serial.println("[SINRIC] Falha ao enviar evento (nova tentativa em 60 s)");
  }
}

// ---------- DHT11 ----------
void lerSensor() {
  float t = sensorDht.readTemperature();
  float u = sensorDht.readHumidity();
  if (isnan(t) || isnan(u)) {
    Serial.println("[DHT] Falha na leitura");
    return;
  }
  temperaturaAtual = t;
  umidadeAtual = u;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  Wire.setClock(400000);
  telaDisponivel = telaOled.begin(SSD1306_SWITCHCAPVCC, ENDERECO_TELA_OLED);
  if (telaDisponivel) {
    telaOled.cp437(true);
    desenharMensagem("Iniciando...", "");
  } else {
    Serial.println("Aviso: Falha ao inicializar display OLED.");
  }

  sensorDht.begin();

  Serial.println("==================================================");
  Serial.println(" Clima Interno + Alexa (Sinric Pro)");
  Serial.println(" - DHT11 lido a cada 2 s, exibido no OLED");
  Serial.println(" - Envio ao Sinric ao mudar ou a cada 5 min (min. 60 s)");
  Serial.println("==================================================");

  iniciarWifi();
  iniciarSinric();

  delay(1000);  // DHT11 precisa de ~1 s após energizar
  lerSensor();
  tempoUltimoDHT = millis();
  desenharPainel();
}

void loop() {
  SinricPro.handle();

  unsigned long agora = millis();

  static wl_status_t ultimoStatusWifi = WL_IDLE_STATUS;
  wl_status_t statusWifi = WiFi.status();
  if ((statusWifi == WL_CONNECTED) != (ultimoStatusWifi == WL_CONNECTED)) {
    if (statusWifi == WL_CONNECTED) {
      Serial.printf("[WIFI] Conectado, IP %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI] Conexão perdida; reconectando...");
    }
  }
  ultimoStatusWifi = statusWifi;

  if (agora - tempoUltimoDHT >= INTERVALO_DHT) {
    tempoUltimoDHT = agora;
    lerSensor();
  }

  enviarLeituraSinric();

  static unsigned long ultimaTela = 0;
  if (agora - ultimaTela >= INTERVALO_TELA) {
    ultimaTela = agora;
    desenharPainel();
  }
}
