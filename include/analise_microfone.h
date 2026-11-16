#pragma once
#include <stdint.h>

// Estatísticas de uma janela de ADC, sem guardar as amostras nem usar FFT.
struct AnaliseMicrofone {
  void adicionar(int leitura) {
    if (leitura < minimo) minimo = leitura;
    if (leitura > maximo) maximo = leitura;
    soma += leitura;
    somaQuadrados += uint64_t(leitura) * leitura;
    if (amostras) {
      int diferenca = leitura - anterior;
      somaDiferencas += int64_t(diferenca) * diferenca;
    }
    anterior = leitura;
    ++amostras;
  }

  int vpp() const { return amostras ? maximo - minimo : 0; }
  bool saturada() const { return amostras && (minimo <= 5 || maximo >= 4090); }

  // Variação entre amostras / energia sem DC, em milésimos.
  // Ondas suaves têm valor baixo; impactos com variação rápida, valor maior.
  // Não é uma estimativa de pitch nem identifica uma palma por si só.
  int textura() const {
    if (amostras < 2) return 0;
    uint64_t energia = amostras * somaQuadrados - soma * soma;
    if (!energia) return 0;
    uint64_t valor = 1000ULL * amostras * somaDiferencas / energia;
    return valor > 4000 ? 4000 : int(valor);
  }

  uint32_t amostras = 0;
 private:
  int minimo = 4095, maximo = 0, anterior = 0;
  uint64_t soma = 0, somaQuadrados = 0, somaDiferencas = 0;
};
