# Monitoramento de Temperatura e Umidade com ESP32

Projeto de Internet das Coisas (IoT) desenvolvido para leitura de dados de temperatura e umidade com microcontrolador ESP32 e exibição dos valores em tempo real em uma interface web.

## O que tem no projeto

- **src/**: Codigo-fonte do ESP32 (leitura do sensor e envio de informacoes).
- **include/**: Configuracoes e dados de conexao da rede Wi-Fi.
- **data/**: Pagina web com graficos para acompanhar as leituras no navegador.
- **mqtt/**: Configuracao do Docker para rodar o servidor local de comunicacao (Mosquitto).

## Como funciona

1. O ESP32 le a temperatura e a umidade atraves de um sensor (DHT11).
2. As leituras sao enviadas periodicamente para um servidor de mensagens (broker MQTT).
3. A interface web se conecta ao servidor e atualiza os valores e graficos na tela em tempo real.

## Como rodar

### 1. Iniciar o servidor de mensagens (Docker)
No terminal, entre na pasta do broker e inicie o container:
```bash
cd mqtt
docker compose up -d
```

### 2. Configurar e gravar o ESP32
1. Abra o arquivo `include/credenciais.h` e coloque o nome e senha da sua rede Wi-Fi.
2. Conecte o ESP32 no computador.
3. No VS Code com a extensao PlatformIO, compile e faca o upload do projeto para a placa.

### 3. Acompanhar as leituras
Com o ESP32 e seu computador conectados na mesma rede, abra o navegador e acesse o endereco IP do ESP32 para ver o painel com as temperaturas e umidades.
