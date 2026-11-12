# Monitor de Clima Interno com Controle Acústico (ESP32, MAX4466 e OLED)

Projeto de IoT para monitoramento de **temperatura e umidade** em tempo real com controle de interface ativado por **palmas ou estalos sonoros**, utilizando microcontrolador **ESP32**, sensor **DHT11**, microfone analógico **MAX4466** e **Display OLED I2C (SSD1306)**.

O firmware opera de forma autônoma e offline, com amostragem contínua de áudio, filtragem contra repiques (debounce acústico) e leitura periódica dos sensores climáticos em segundo plano.

---

## 🛠️ Hardware e Pinagem

| Componente | Pino do Componente | Pino do ESP32 | Descrição |
| :--- | :--- | :--- | :--- |
| **Microfone MAX4466** | **OUT** | **GPIO 34** (ADC1_CH6) | Sinal analógico de áudio amplificado |
| | VCC | 3.3V | Alimentação |
| | GND | GND | Terra |
| **Sensor DHT11** | Dados (DATA / OUT) | **GPIO 13** | Sinal digital de temperatura e umidade |
| | VCC | 3.3V ou 5V | Alimentação |
| | GND | GND | Terra |
| **Display OLED 0.96"** | **SDA** | **GPIO 21** | Barramento de dados I2C |
| *(SSD1306 / 128x64)* | **SCL / SCK** | **GPIO 22** | Clock I2C |
| | VCC | 3.3V | Alimentação |
| | GND | GND | Terra |

> **Nota I2C:** Endereço padrão da tela OLED: `0x3C`.

---

## 📱 Estados da Interface e Funcionamento

A interface do display OLED alterna entre três modos dinâmicos acionados por som:

### 1. Modo Standby (Repouso)
O display bicolor reserva as linhas **0–15 (amarelo)** para o título
`CLIMA INTERNO`. A área **16–63 (azul)** mostra um ícone climático,
`PRONTO` e os atalhos `1 palma: temperatura` / `2 palmas: umidade`.
Os textos respeitam a divisão física das cores, sem atravessá-la.

### 2. Tela de Temperatura (1 Palma / Estalo)
Ao detectar um pulso sonoro nítido, exibe a temperatura atual com indicador regressivo de tempo:
```text
+------------------------------+
|        CLIMA INTERNO         |
|         TEMPERATURA          |
|           26.5 C             |
| [==========================] |
+------------------------------+
```

### 3. Tela de Umidade (2 Palmas / Estalos Rápidos)
Ao detectar dois pulsos sonoros consecutivos dentro da janela de confirmação, exibe a umidade relativa:
```text
+------------------------------+
|        CLIMA INTERNO         |
|           UMIDADE            |
|            48 %              |
| [==========================] |
+------------------------------+
```

* **Retorno Automático:** Após 5 segundos de exibição, a tela retorna automaticamente para o logotipo de Standby.

---

## ⚙️ Ajuste de Sensibilidade do Microfone

A sensibilidade pode ser calibrada de duas maneiras:

1. **Via Código (`src/main.cpp`):**
   * Ajuste o valor da constante `LIMIAR_PALMA_VPP`:
     * `850 - 1050`: Sensibilidade mais alta (capta sons leves a moderados).
     * `1100 - 1400`: Sensibilidade balanceada (recomendada para quartos/salas).
     * `1500+`: Exige palmas mais fortes ou próximas ao microfone.
2. **Via Hardware (Trimpot do MAX4466):**
   * O trimpot na parte traseira da placa do MAX4466 regula o ganho de 25x a 125x:
     * **Sentido anti-horário:** diminui o ganho (menos sensível).
     * **Sentido horário:** aumenta o ganho (mais sensível).

---

## 📁 Estrutura do Repositório

* **`src/main.cpp`**: Firmware principal em C++ (PlatformIO / Arduino Core):
  * Amostragem contínua do microfone na GPIO34 (janela de 25 ms com medição de pico a pico `Vpp`).
  * Máquina de estados para contagem de palmas e temporizador de retorno ao standby.
  * Leitura não-bloqueante do DHT11 a cada 2 segundos.
  * Renderização visual no display SSD1306.
* **`platformio.ini`**: Configurações de compilação e dependências das bibliotecas Adafruit.
* **`data/`**: Interface web estática complementar.
* **`backend/`**: Servidor complementar em Python com SQLite para histórico.
* **`mqtt/`**: Configurações adicionais de broker MQTT via Docker Compose.

---

## 🚀 Como Compilar e Gravar

1. Conecte o ESP32 ao computador via USB.
2. Compile e envie o firmware utilizando o PlatformIO CLI:
   ```bash
   pio run -t upload
   ```
3. Para acompanhar o log de eventos e depuração do microfone via monitor serial (115200 baud):
   ```bash
   pio device monitor
   ```

## Diagnóstico do microfone

O diagnóstico do microfone fica apenas na serial. O OLED exibe a interface
de clima, sem níveis ou limiares. Nas telas de temperatura e umidade, a
barra inferior mostra o tempo restante até o standby (5 s).

A serial a 115200 baud publica `[MIC]` a cada aproximadamente 100 ms:
`pico`, `fundo`, `limiar`, `armado`, `pulso`, `contagem`,
`n` (janelas amostradas), `clip` (janelas próximas dos limites do ADC) e
`gap` (maior intervalo em ms entre janelas) e `perdidas` (janelas descartadas
por fila cheia). `[INICIO]`, `[SOM]`, `[REJEITADO]`, `[ACAO]` e `[LACUNA]`
ajudam a rastrear a decisão. A leitura do ADC ocorre em uma tarefa separada,
com janelas de 4 ms e uma pausa de um tick entre elas; a fila comporta
64 janelas. Lacunas acima de 25 ms ou perdas cancelam a sequência pendente.

O detector em `include/detector_palmas.h` aprende o fundo somente após
300 ms de silêncio estável, limita o limiar entre 700 e 1000 e o congela
durante a sequência. Exige ataque em até 25 ms após o silêncio, queda de
amplitude sustentada por 12 ms e duração de até aproximadamente 120 ms.
O intervalo entre inícios de palmas deve ficar entre 120 e 600 ms. Esses
critérios temporais ainda podem aceitar outros impactos e sílabas curtas;
a saturação sinalizada por `clip` precisa ser avaliada também no ganho do MAX4466.

Para executar os testes sintéticos no computador (requer `g++`):

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_palmas/test_detector.cpp -o /tmp/test_detector_palmas
/tmp/test_detector_palmas
```

Os testes verificam contagem e temporização; não medem precisão acústica.
O log anterior agrega picos em 100 ms e não permite reconstruir os pulsos
originais para comparar a precisão das versões.

Para calibrar, registre silêncio, fala sem palmas, palmas isoladas e pares
com intervalo de 200–400 ms; anote o momento de cada teste para comparar
com os logs. Feche o monitor serial antes de gravar outro firmware.
