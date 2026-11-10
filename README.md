# Monitoramento de Clima Interno com ESP32

Projeto completo de Internet das Coisas (IoT) para monitoramento de temperatura e umidade em tempo real, integrando microcontrolador **ESP32**, sensor **DHT11**, display **OLED I2C (SSD1306)**, comunicação via **MQTT** e interface web com persistência em banco de dados **SQLite**.

---

## 🛠️ Hardware e Pinagem

| Componente | Pino do Componente | Pino do ESP32 | Observação |
| :--- | :--- | :--- | :--- |
| **Sensor DHT11** | Dados (DATA / OUT) | **GPIO 13** | Sinal digital |
| | VCC | 3.3V ou 5V | Alimentação |
| | GND | GND | Terra comum |
| **Display OLED 0.96"** | **SDA** | **GPIO 21** | Barramento I2C |
| *(SSD1306 / 128x64)* | **SCL / SCK** | **GPIO 22** | Barramento I2C |
| | VCC | 3.3V | Alimentação |
| | GND | GND | Terra comum |

> **Endereço I2C do display:** `0x3C`

---

## 🖥️ Layout do Display OLED

O display OLED de 128x64 pixels exibe as informações formatadas no layout:

```text
+------------------------------+
|        CLIMA INTERNO         |
|------------------------------|
| Temp:   22.6 C               |
|------------------------------|
| Umid:   44 %                 |
+------------------------------+
```

---

## 📁 Estrutura do Projeto

* **`src/main.cpp`**: Código-fonte C++ embarcado do ESP32:
  * Leitura do sensor DHT11 a cada 1 segundo.
  * Atualização da tela OLED em tempo real.
  * Broker MQTT embarcado (`PicoMQTT` na porta TCP `1883` e WebSocket `8080`).
  * Servidor web HTTP na porta `80` para servir os arquivos do LittleFS.
  * Ponto de acesso Wi-Fi de contingência (`ESP32-Sensor` / senha `12345678`).
* **`include/credenciais.h`**: Configuração do SSID e senha da sua rede Wi-Fi local.
* **`data/`**: Interface Web responsiva com gráficos em tempo real (`index.html` e biblioteca `mqtt.min.js`).
* **`backend/`**: Servidor Python (`app.py`) com banco de dados SQLite (`dados.db`) para armazenamento e histórico por períodos (tempo real, dia, semana, mês).
* **`mqtt/`**: Arquivos de configuração e `docker-compose.yml` para execução de broker Eclipse Mosquitto externo (opcional).

---

## 📡 Tópicos MQTT

| Tópico | Descrição | Exemplo de Carga |
| :--- | :--- | :--- |
| `sensor/temperatura` | Leitura da temperatura em graus Celsius | `24.5` |
| `sensor/umidade` | Leitura da umidade relativa do ar (%) | `42` |

---

## 🚀 Como Executar

### 1. Gravar o ESP32 (Firmware e Sistema de Arquivos)

1. Ajuste suas credenciais de Wi-Fi no arquivo `include/credenciais.h`.
2. Compile e faça o upload do firmware:
   ```bash
   pio run -t upload
   ```
3. Grave a interface web na memória flash (LittleFS):
   ```bash
   pio run -t uploadfs
   ```

### 2. Acessar a Interface Web do ESP32
Ao ligar a placa, o IP atribuído será exibido no Monitor Serial (`115200` baud).
* Pelo navegador no mesmo Wi-Fi: `http://<IP_DO_ESP32>`
* Caso não conecte ao Wi-Fi, conecte-se à rede `ESP32-Sensor` (senha `12345678`) e acesse `http://192.168.4.1`.

### 3. Executar o Servidor de Histórico (Backend Python + SQLite)
Na pasta do projeto:
```bash
python3 backend/app.py
```
O servidor iniciará na porta `5000` (`http://localhost:5000`), coletando leituras e gerando histórico no banco SQLite local.
