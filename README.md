# Monitoramento de Clima Interno com ESP32 e Display OLED

Projeto de IoT simplificado, limpo e direto ao ponto para leitura de **temperatura e umidade** em tempo real utilizando microcontrolador **ESP32**, sensor **DHT11** e **Display OLED I2C (SSD1306)**.

O firmware é autônomo, não depende de conexões de rede externas ou brokers MQTT, focando em máxima estabilidade, legibilidade de código e velocidade de inicialização.

---

## 🛠️ Hardware e Pinagem

| Componente | Pino do Componente | Pino do ESP32 | Descrição |
| :--- | :--- | :--- | :--- |
| **Sensor DHT11** | Dados (DATA / OUT) | **GPIO 13** | Sinal digital de leitura |
| | VCC | 3.3V ou 5V | Alimentação |
| | GND | GND | Terra |
| **Display OLED 0.96"** | **SDA** | **GPIO 21** | Dados I2C |
| *(SSD1306 / 128x64)* | **SCL / SCK** | **GPIO 22** | Clock I2C |
| | VCC | 3.3V | Alimentação |
| | GND | GND | Terra |

> **Endereço I2C do display:** `0x3C`

---

## 🖥️ Layout do Display OLED

As medições são atualizadas a cada 1 segundo no display de 128x64 pixels:

```text
+------------------------------+
|        CLIMA INTERNO         |
|------------------------------|
| Temp:   23.8 C               |
|------------------------------|
| Umid:   40 %                 |
+------------------------------+
```

---

## 📁 Estrutura do Repositório

* **`src/main.cpp`**: Código C++ do firmware:
  * Leitura e validação do sensor DHT11.
  * Formatação e atualização direta da tela OLED.
  * Código 100% em português, limpo e sem dependências desnecessárias.
* **`platformio.ini`**: Configuração do projeto PlatformIO com as bibliotecas essenciais (`Adafruit SSD1306`, `Adafruit GFX` e `DHT sensor library`).
* **`data/`**: Interface web complementar (HTML/JS com gráficos).
* **`backend/`**: Servidor complementar em Python com SQLite para histórico e telemetria via API.
* **`mqtt/`**: Configuração Docker Compose com Mosquitto MQTT.

---

## 🚀 Como Compilar e Gravar

1. Conecte o ESP32 ao computador via cabo USB (porta `/dev/ttyACM0` ou equivalente).
2. Compile e grave diretamente pela linha de comando ou VS Code:
   ```bash
   pio run -t upload
   ```
3. Para monitorar as leituras via terminal serial (115200 baud):
   ```bash
   pio device monitor
   ```
