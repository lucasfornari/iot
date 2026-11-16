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

A detecção considera volume relativo ao ambiente, ataque rápido, variação
entre amostras e decaimento. Não calcula pitch: a medida `textura` ajuda a
rejeitar sinais de variação suave, mas não identifica a origem de um som.

1. **Via Código (`include/detector_palmas.h`):**
   * `LIMIAR_MINIMO = 700` define o volume mínimo. O limiar efetivo acompanha
     o fundo com fator 2,5 e fica entre 700 e 3500 unidades do ADC.
   * `PICO_MINIMO = 2800` confirma que o pulso chegou ao volume esperado de
     uma palma próxima. Foi calibrado com este MAX4466; palmas fracas ou
     distantes podem ser descartadas. Reavalie esse valor ao alterar o ganho.
   * `TEXTURA_MINIMA = 800` exige variação rápida no ataque. Aumentar esse
     valor torna o filtro mais restritivo e também pode rejeitar palmas.
   * O detector calibra durante o primeiro segundo; mantenha o ambiente
     representativo do uso, sem bater palmas nesse intervalo.
2. **Via Hardware (Trimpot do MAX4466):**
   * O trimpot na parte traseira da placa do MAX4466 regula o ganho de 25x a 125x:
     * Ajuste aos poucos, comparando `pico`, `fundo` e `clip` na serial.
     * Se `clip` ocorrer frequentemente durante fala ou ruído ambiente,
       reduza o ganho. Aumentar o volume não recupera detalhes já saturados.

---

## 📁 Estrutura do Repositório

* **`src/main.cpp`**: Firmware principal em C++ (PlatformIO / Arduino Core):
  * Aquisição do microfone na GPIO34 em tarefa separada, com janelas de 4 ms e aproximadamente 8 kHz dentro de cada janela.
  * Máquina de estados para contagem de palmas e temporizador de retorno ao standby.
  * Leitura do DHT11 a cada 2 segundos, enquanto a tarefa do microfone continua.
  * Renderização visual no display SSD1306.
* **`platformio.ini`**: Configurações de compilação e dependências das bibliotecas Adafruit.
* **`include/analise_microfone.h`**: Extração de Vpp, saturação e textura a partir das amostras do ADC.
* **`include/detector_palmas.h`**: Detecção e contagem temporal de palmas.
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
`pico`, `textura` (da janela de maior pico), `fundo`, `limiar`, `armado`, `pulso`, `contagem`,
`n` (janelas amostradas), `clip` (janelas próximas dos limites do ADC) e
`gap` (maior intervalo em ms entre janelas) e `perdidas` (janelas descartadas
por fila cheia). `[INICIO]`, `[SOM]`, `[REJEITADO]`, `[ACAO]` e `[LACUNA]`
ajudam a rastrear a decisão. A leitura do ADC ocorre em uma tarefa separada,
com janelas de 4 ms e uma pausa de um tick entre elas; a fila comporta
64 janelas. Lacunas acima de 25 ms ou perdas cancelam a sequência pendente.

Para registrar as características de todas as janelas do MAX4466,
envie `c` pelo monitor serial. Durante três minutos, o firmware acrescenta
linhas CSV `J,tempo,sequencia,vpp,textura,clip,acao`; envie `x` para encerrar
antes. Salve a saída serial para analisar as decisões do detector. Esse
registro contém medidas do sinal, sem áudio reproduzível, e mantém a
detecção e o OLED funcionando.

O detector aprende o fundo fora de pulsos, da pausa entre duas palmas e
dos bloqueios após uma dupla ou um candidato rejeitado. Assim consegue rearmar mesmo com ruído
constante acima do antigo teto fixo de silêncio. O limiar fica congelado
durante a sequência para uma primeira palma forte não dificultar a segunda.

O ataque precisa superar o limiar, crescer pelo menos 1,8 vez em relação
à janela anterior e começar em até 15 ms após o nível de repouso. A textura
é avaliada nos primeiros 12 ms do pulso. O som deve começar a cair antes
de 70 ms e permanecer abaixo do nível de retorno por 15 ms; pulsos que
atingem 100 ms são descartados. O nível de retorno considera o fundo e
40% do pico, permitindo a cauda de uma palma forte. Essa tolerância também
é verificada com envelopes registrados no MAX4466 durante a calibração.

O intervalo entre inícios de palmas deve ficar entre 80 e 600 ms, com
20 ms de repouso relativo ao ambiente para rearmar o detector. A umidade aparece assim que
a segunda palma é validada; uma palma isolada aguarda 600 ms para confirmar
a temperatura. Após uma dupla, há 180 ms de bloqueio contra ecos; após um
candidato rejeitado, 80 ms evitam rearmar no meio da mesma rajada de ruído.
Um candidato inválido não apaga uma palma já confirmada. Novos
comandos continuam sendo aceitos enquanto uma leitura está no OLED e
reiniciam os cinco segundos de exibição.

`[REJEITADO] motivo=longo` indica duração excessiva; `motivo=tonal` indica
textura insuficiente no ataque; `motivo=fraco` indica pico abaixo de 2800.
Esses filtros ainda podem aceitar outros
impactos ou certos sons de fala. A saturação sinalizada por `clip` precisa
ser avaliada também no ganho do MAX4466.

Para executar os testes sintéticos no computador (requer `g++`):

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_palmas/test_detector.cpp -o /tmp/test_detector_palmas
/tmp/test_detector_palmas
```

Os testes verificam extração do sinal, contagem, temporização, ruído de
fundo elevado, sinal tonal sintético, cauda forte, recuperação, ecos,
lacunas e overflow do relógio. Não medem precisão acústica com vozes e
palmas reais. Os logs agregados em 100 ms não permitem reconstruir as
amostras originais para comparar a precisão das versões.

Para calibrar, registre silêncio, fala sem palmas, palmas isoladas e pares
com intervalo de 200–400 ms; anote o momento de cada teste para comparar
com os logs. Feche o monitor serial antes de gravar outro firmware.
