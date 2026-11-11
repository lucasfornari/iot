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
#define PINO_MIC            34
#define LARGURA_TELA_OLED   128
#define ALTURA_TELA_OLED    64
#define ENDERECO_TELA_OLED  0x3C

// ---------- Configurações de Detecção Sonora (Palmas/Estalos) ----------
const int LIMIAR_PALMA_VPP         = 1050;   // Amplitude mínima para considerar palma/estalo
const unsigned long REFRATARIO_MS  = 180;   // Tempo mínimo entre duas palmas (evita eco da mesma palma)
const unsigned long JANELA_CONFIRM = 450;   // Tempo máximo após última palma para fechar a contagem
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

int palmasContadas = 0;
unsigned long tempoUltimaPalma = 0;
unsigned long tempoInicioExibicao = 0;

// ---------- Funções de Interface Gráfica ----------

void desenharStandby() {
  telaOled.clearDisplay();

  // Moldura decorativa estilizada
  telaOled.drawRect(8, 6, 112, 52, SSD1306_WHITE);
  telaOled.drawRect(10, 8, 108, 48, SSD1306_WHITE);

  // Logo / Título centralizado
  telaOled.setTextColor(SSD1306_WHITE);
  telaOled.setTextSize(2);
  
  // "CLIMA" centralizado (5 chars * 12px = 60px -> X = (128 - 60)/2 = 34)
  telaOled.setCursor(34, 16);
  telaOled.print("CLIMA");

  // "INTERNO" centralizado (7 chars * 12px = 84px -> X = (128 - 84)/2 = 22)
  telaOled.setCursor(22, 34);
  telaOled.print("INTERNO");

  telaOled.display();
}

void desenharTemperatura(float temp, unsigned long tempoRestanteMs) {
  telaOled.clearDisplay();
  telaOled.setTextColor(SSD1306_WHITE);

  // Cabeçalho
  telaOled.setTextSize(1);
  telaOled.setCursor(25, 2);
  telaOled.print("CLIMA INTERNO");
  telaOled.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Subtítulo
  telaOled.setCursor(31, 16);
  telaOled.print("TEMPERATURA");

  // Valor em destaque
  telaOled.setTextSize(3);
  // Centraliza o texto "XX.X C"
  telaOled.setCursor(14, 28);
  telaOled.printf("%.1f C", temp);

  // Barra regressiva de tempo até retornar ao standby
  int barra = map(tempoRestanteMs, 0, TEMPO_EXIBICAO, 0, 120);
  telaOled.drawRect(4, 56, 120, 5, SSD1306_WHITE);
  if (barra > 0) {
    telaOled.fillRect(4, 56, constrain(barra, 0, 120), 5, SSD1306_WHITE);
  }

  telaOled.display();
}

void desenharUmidade(float umid, unsigned long tempoRestanteMs) {
  telaOled.clearDisplay();
  telaOled.setTextColor(SSD1306_WHITE);

  // Cabeçalho
  telaOled.setTextSize(1);
  telaOled.setCursor(25, 2);
  telaOled.print("CLIMA INTERNO");
  telaOled.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Subtítulo
  telaOled.setCursor(43, 16);
  telaOled.print("UMIDADE");

  // Valor em destaque
  telaOled.setTextSize(3);
  telaOled.setCursor(28, 28);
  telaOled.printf("%.0f %%", umid);

  // Barra regressiva de tempo até retornar ao standby
  int barra = map(tempoRestanteMs, 0, TEMPO_EXIBICAO, 0, 120);
  telaOled.drawRect(4, 56, 120, 5, SSD1306_WHITE);
  if (barra > 0) {
    telaOled.fillRect(4, 56, constrain(barra, 0, 120), 5, SSD1306_WHITE);
  }

  telaOled.display();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  analogSetAttenuation(ADC_11db);
  pinMode(PINO_MIC, INPUT);

  // Inicializa barramento I2C e tela OLED
  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
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
}

void loop() {
  unsigned long agora = millis();

  // 1. Amostragem rápida do microfone MAX4466 (janela de 25 ms)
  const unsigned long JANELA_AMOSTRAGEM_MS = 25;
  unsigned long inicioJanela = agora;
  unsigned int pMin = 4095;
  unsigned int pMax = 0;

  while (millis() - inicioJanela < JANELA_AMOSTRAGEM_MS) {
    int leitura = analogRead(PINO_MIC);
    if (leitura < pMin) pMin = leitura;
    if (leitura > pMax) pMax = leitura;
    delayMicroseconds(50);
  }

  int vpp = (pMax >= pMin) ? (pMax - pMin) : 0;
  agora = millis();

  // 2. Detecção de Pico Sonoro (Palma / Estalo)
  if (vpp >= LIMIAR_PALMA_VPP) {
    if (agora - tempoUltimaPalma > REFRATARIO_MS) {
      palmasContadas++;
      tempoUltimaPalma = agora;
      Serial.printf("[SOM] Pico detectado! Vpp: %d -> Palma #%d\n", vpp, palmasContadas);
    }
  }

  // 3. Confirmação do número de palmas após silêncio de validação
  if (palmasContadas > 0 && (agora - tempoUltimaPalma > JANELA_CONFIRM)) {
    if (palmasContadas == 1) {
      estadoAtual = ESTADO_TEMPERATURA;
      tempoInicioExibicao = agora;
      Serial.println("[AÇÃO] 1 Palma -> Exibindo TEMPERATURA");
    } else if (palmasContadas >= 2) {
      estadoAtual = ESTADO_UMIDADE;
      tempoInicioExibicao = agora;
      Serial.printf("[AÇÃO] %d Palmas -> Exibindo UMIDADE\n", palmasContadas);
    }
    palmasContadas = 0;
  }

  // 4. Temporizador de retorno para Standby
  if (estadoAtual != ESTADO_STANDBY) {
    unsigned long tempoDecorrido = agora - tempoInicioExibicao;
    if (tempoDecorrido >= TEMPO_EXIBICAO) {
      estadoAtual = ESTADO_STANDBY;
      desenharStandby();
      Serial.println("[STANDBY] Retornando ao logo CLIMA INTERNO");
    } else {
      unsigned long tempoRestante = TEMPO_EXIBICAO - tempoDecorrido;
      if (estadoAtual == ESTADO_TEMPERATURA) {
        desenharTemperatura(temperaturaAtual, tempoRestante);
      } else if (estadoAtual == ESTADO_UMIDADE) {
        desenharUmidade(umidadeAtual, tempoRestante);
      }
    }
  }

  // 5. Atualização periódica do sensor DHT a cada 2 segundos (não-bloqueante)
  if (agora - tempoUltimoDHT >= 2000) {
    tempoUltimoDHT = agora;
    float t = sensorDht.readTemperature();
    float u = sensorDht.readHumidity();
    if (!isnan(t)) temperaturaAtual = t;
    if (!isnan(u)) umidadeAtual = u;
  }
}
