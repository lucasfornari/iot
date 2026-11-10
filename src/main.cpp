#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Configurações de Hardware ----------
#define PINO_SENSOR_DHT     13
#define TIPO_SENSOR_DHT     DHT11
#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22
#define LARGURA_TELA_OLED   128
#define ALTURA_TELA_OLED    64
#define ENDERECO_TELA_OLED  0x3C

// ---------- Intervalo de Leitura ----------
const unsigned long INTERVALO_LEITURA_MS = 1000;

// ---------- Instâncias Globais ----------
DHT sensorDht(PINO_SENSOR_DHT, TIPO_SENSOR_DHT);
Adafruit_SSD1306 telaOled(LARGURA_TELA_OLED, ALTURA_TELA_OLED, &Wire, -1);

unsigned long tempoUltimaLeitura = 0;

void atualizarTela(float temperatura, float umidade) {
  telaOled.clearDisplay();

  // Cabeçalho
  telaOled.setTextSize(1);
  telaOled.setTextColor(SSD1306_WHITE);
  telaOled.setCursor(25, 3);
  telaOled.print("CLIMA INTERNO");
  telaOled.drawLine(0, 13, 127, 13, SSD1306_WHITE);

  // Bloco de Temperatura
  telaOled.setCursor(4, 24);
  telaOled.print("Temp:");
  telaOled.setTextSize(2);
  telaOled.setCursor(44, 20);
  telaOled.printf("%.1f C", temperatura);

  // Linha divisória horizontal
  telaOled.drawLine(4, 38, 123, 38, SSD1306_WHITE);

  // Bloco de Umidade
  telaOled.setTextSize(1);
  telaOled.setCursor(4, 47);
  telaOled.print("Umid:");
  telaOled.setTextSize(2);
  telaOled.setCursor(44, 43);
  telaOled.printf("%.0f %%", umidade);

  telaOled.display();
}

void setup() {
  Serial.begin(115200);

  // Inicializa comunicação I2C e tela OLED
  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  if (telaOled.begin(SSD1306_SWITCHCAPVCC, ENDERECO_TELA_OLED)) {
    telaOled.clearDisplay();
    telaOled.setTextSize(1);
    telaOled.setTextColor(SSD1306_WHITE);
    telaOled.setCursor(20, 28);
    telaOled.print("Iniciando...");
    telaOled.display();
  } else {
    Serial.println("Aviso: Falha ao inicializar a tela OLED.");
  }

  sensorDht.begin();
  Serial.println("Monitor de Clima Interno iniciado com sucesso!");
}

void loop() {
  if (millis() - tempoUltimaLeitura >= INTERVALO_LEITURA_MS) {
    tempoUltimaLeitura = millis();

    float temperatura = sensorDht.readTemperature();
    float umidade = sensorDht.readHumidity();

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("Falha na leitura do sensor DHT11.");
      return;
    }

    Serial.printf("Temperatura: %.1f C | Umidade: %.0f %%\n", temperatura, umidade);
    atualizarTela(temperatura, umidade);
  }
}