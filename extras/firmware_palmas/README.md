# Firmware anterior: controle por palmas (MAX4466)

Este `main.cpp` é o firmware offline com detecção de palmas (1 palma =
temperatura, 2 palmas = umidade). Ele está fora de `src/`, então não entra
na compilação atual (Alexa/Sinric Pro). Nada foi apagado:

- `include/analise_microfone.h` e `include/detector_palmas.h` continuam no lugar;
- os testes sintéticos em `test/test_palmas/` continuam rodando.

Para voltar a usar este firmware, troque o `src/main.cpp` por este arquivo
(ex.: `cp extras/firmware_palmas/main.cpp src/main.cpp`). Ele não usa
Sinric nem Wi-Fi; a dependência do SinricPro pode ficar no `platformio.ini`.
O microfone usa a GPIO34 (ADC1), que funciona junto com o Wi-Fi caso as
duas funções sejam unidas no futuro.
