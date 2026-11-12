#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "detector_palmas.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ---------- Configurações de Hardware ----------
#define PINO_SENSOR_DHT     13
#define TIPO_SENSOR_DHT     DHT11
#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22
#define PINO_MIC            34
#define LARGURA_TELA_OLED   128
#define ALTURA_TELA_OLED    64
#define ENDERECO_TELA_OLED  0x3C

// ---------- Configurações de Detecção Sonora (Palmas/Estalos) ----------
const unsigned long TEMPO_EXIBICAO = 5000;  // Tempo de exibição da tela antes de voltar ao standby (5s)

// ---------- Estados da Tela ----------
enum EstadoTela {
  ESTADO_STANDBY,
  ESTADO_TEMPERATURA,
  ESTADO_UMIDADE
};

// ---------- Instâncias Globais ----------
DHT sensorDht(PINO_SENSOR_DHT, TIPO_SENSOR_DHT);
Adafruit_SSD1306 telaOled(LARGURA_TELA_OLED, ALTURA_TELA_OLED, &Wire, -1);

EstadoTela estadoAtual = ESTADO_STANDBY;

float temperaturaAtual = 0.0;
float umidadeAtual = 0.0;
unsigned long tempoUltimoDHT = 0;

unsigned long tempoInicioExibicao = 0;

// Aquisição separada: OLED/serial/DHT não suspendem o leitor do ADC.
struct JanelaMic {
  uint32_t tempo;
  uint32_t sequencia;
  int vpp;
  bool saturada;
};
QueueHandle_t filaMic;
DetectorPalmas detector;

void amostrarMicrofone(void *) {
  uint32_t sequencia = 0;
  for (;;) {
    int minimo = 4095, maximo = 0;
    uint32_t inicio = micros();
    while (uint32_t(micros() - inicio) < 4000) {
      int leitura = analogRead(PINO_MIC);
      if (leitura < minimo) minimo = leitura;
      if (leitura > maximo) maximo = leitura;
      delayMicroseconds(50);
    }
    JanelaMic janela{millis(), sequencia++, maximo - minimo,
                      minimo <= 5 || maximo >= 4090};
    // Sem bloqueio: uma perda fica identificável pelo número de sequência.
    xQueueSend(filaMic, &janela, 0);
    vTaskDelay(1);  // Cede CPU ao sistema entre janelas de 4 ms.
  }
}

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
  // Separador já na área azul; deixa a divisão das cores respirar.
  telaOled.drawFastHLine(8, ALTURA_FAIXA_AMARELA + 1, 112, SSD1306_WHITE);
}

void desenharStandby() {
  desenharCabecalho();

  // Símbolo climático inteiramente na área azul, ao lado do estado.
  telaOled.drawCircle(15, 27, 5, SSD1306_WHITE);
  telaOled.drawFastVLine(15, 19, 2, SSD1306_WHITE);
  telaOled.drawFastHLine(6, 27, 2, SSD1306_WHITE);
  telaOled.drawFastHLine(22, 27, 2, SSD1306_WHITE);
  telaOled.fillCircle(17, 34, 4, SSD1306_WHITE);
  telaOled.fillCircle(23, 31, 5, SSD1306_WHITE);
  telaOled.fillCircle(29, 35, 4, SSD1306_WHITE);
  telaOled.fillRect(17, 34, 13, 5, SSD1306_WHITE);

  telaOled.setTextSize(2);
  telaOled.setCursor(45, 25);
  telaOled.print("PRONTO");
  desenharTextoCentralizado("1 palma: temperatura", 45, 1);
  desenharTextoCentralizado("2 palmas: umidade", 56, 1);
  telaOled.display();
}

void desenharLeitura(const char *titulo, const char *valor) {
  desenharCabecalho();
  desenharTextoCentralizado(titulo, 21, 1);
  desenharTextoCentralizado(valor, 34, 2);

  // Progresso discreto na área azul; esvazia em cinco segundos.
  unsigned long decorrido = millis() - tempoInicioExibicao;
  unsigned long restante = decorrido >= TEMPO_EXIBICAO ? 0 : TEMPO_EXIBICAO - decorrido;
  int largura = (restante * 110UL) / TEMPO_EXIBICAO;
  telaOled.drawRoundRect(8, 57, 112, 5, 2, SSD1306_WHITE);
  if (largura > 0) telaOled.fillRect(9, 58, largura, 3, SSD1306_WHITE);
  telaOled.display();
}

void desenharTemperatura(float temp) {
  char valor[20];
  snprintf(valor, sizeof(valor), "%.1f C", temp);
  desenharLeitura("TEMPERATURA", valor);
}

void desenharUmidade(float umid) {
  char valor[20];
  snprintf(valor, sizeof(valor), "%.0f %%", umid);
  desenharLeitura("UMIDADE", valor);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  analogSetAttenuation(ADC_11db);
  pinMode(PINO_MIC, INPUT);

  // Inicializa barramento I2C e tela OLED
  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  Wire.setClock(400000);
  if (telaOled.begin(SSD1306_SWITCHCAPVCC, ENDERECO_TELA_OLED)) {
    desenharStandby();
  } else {
    Serial.println("Aviso: Falha ao inicializar display OLED.");
  }

  sensorDht.begin();

  // Leitura inicial do sensor DHT11
  delay(1000);
  float t = sensorDht.readTemperature();
  float u = sensorDht.readHumidity();
  if (!isnan(t)) temperaturaAtual = t;
  if (!isnan(u)) umidadeAtual = u;

  Serial.println("==================================================");
  Serial.println(" Sistema Clima Interno Ativo!");
  Serial.println(" - 1 Palma  -> Exibe TEMPERATURA");
  Serial.println(" - 2 Palmas -> Exibe UMIDADE");
  Serial.println(" - Retorna automaticamente ao Standby apos 5s");
  Serial.println("==================================================");
  filaMic = xQueueCreate(64, sizeof(JanelaMic));
  if (!filaMic || xTaskCreatePinnedToCore(amostrarMicrofone, "microfone", 3072,
                                         nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("[ERRO] Falha ao iniciar aquisicao do microfone");
    while (true) delay(1000);
  }
}

void loop() {
  unsigned long agora = millis();

  static uint32_t ultimoLog = 0, ultimaJanela = 0, ultimaSequencia = 0;
  static uint32_t maiorIntervalo = 0, perdidas = 0;
  static bool recebeuJanela = false;
  static int picoLog = 0, saturadas = 0, janelas = 0;
  JanelaMic janela;
  while (xQueueReceive(filaMic, &janela, 0) == pdTRUE) {
    if (recebeuJanela) {
      uint32_t intervalo = janela.tempo - ultimaJanela;
      maiorIntervalo = max(maiorIntervalo, intervalo);
      uint32_t faltantes = janela.sequencia - ultimaSequencia - 1;
      if (faltantes || intervalo > 25) {
        perdidas += faltantes;
        detector.interromper();
        Serial.printf("[LACUNA] t=%lu gap=%lu perdidas=%lu\n",
                      janela.tempo, intervalo, faltantes);
      }
    }
    recebeuJanela = true;
    ultimaJanela = janela.tempo;
    ultimaSequencia = janela.sequencia;
    auto resultado = detector.processar(janela.vpp, janela.tempo);
    if (resultado.inicio) {
      Serial.printf("[INICIO] t=%lu vpp=%d limiar=%.0f\n",
                    janela.tempo, janela.vpp, detector.limiar);
    }
    if (resultado.aceito) {
      Serial.printf("[SOM] t=%lu pico=%d duracao=%lu contagem=%d\n",
                    janela.tempo, detector.pico, resultado.duracao, detector.contagem);
    }
    if (resultado.rejeitado) {
      Serial.printf("[REJEITADO] t=%lu motivo=longo pico=%d duracao=%lu\n",
                    janela.tempo, detector.pico, resultado.duracao);
    }
    if (resultado.acao) {
      estadoAtual = resultado.acao == 1 ? ESTADO_TEMPERATURA : ESTADO_UMIDADE;
      tempoInicioExibicao = agora;
      Serial.printf("[ACAO] t=%lu palmas=%d\n", janela.tempo, resultado.acao);
    }
    picoLog = max(picoLog, janela.vpp);
    saturadas += janela.saturada;
    janelas++;
  }
  if (agora - ultimoLog >= 100 && janelas) {
    Serial.printf("[MIC] t=%lu pico=%d fundo=%.0f limiar=%.0f armado=%d pulso=%d contagem=%d n=%d clip=%d gap=%lu perdidas=%lu\n",
                  ultimaJanela, picoLog, detector.fundo, detector.limiar,
                  detector.armado, detector.pulso, detector.contagem,
                  janelas, saturadas, maiorIntervalo, perdidas);
    ultimoLog = agora;
    picoLog = saturadas = janelas = 0;
    maiorIntervalo = perdidas = 0;
  }

  if (estadoAtual != ESTADO_STANDBY && agora - tempoInicioExibicao >= TEMPO_EXIBICAO) {
    estadoAtual = ESTADO_STANDBY;
    Serial.println("[STANDBY] Retornando ao logo CLIMA INTERNO");
  }
  // Atualiza OLED a 10 Hz em vez de bloquear cada janela de áudio.
  static unsigned long ultimaTela = 0;
  if (agora - ultimaTela >= 100) {
    ultimaTela = agora;
    if (estadoAtual == ESTADO_TEMPERATURA) {
      desenharTemperatura(temperaturaAtual);
    } else if (estadoAtual == ESTADO_UMIDADE) {
      desenharUmidade(umidadeAtual);
    } else {
      desenharStandby();
    }
  }

  // Leitura DHT periódica; a tarefa do microfone continua durante a chamada.
  if (agora - tempoUltimoDHT >= 2000) {
    tempoUltimoDHT = agora;
    float t = sensorDht.readTemperature();
    float u = sensorDht.readHumidity();
    if (!isnan(t)) temperaturaAtual = t;
    if (!isnan(u)) umidadeAtual = u;
  }
}
