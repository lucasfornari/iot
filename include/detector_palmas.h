#pragma once
#include <stdint.h>

// Detector temporal sobre Vpp: limiares em unidades do ADC, tempos em ms.
class DetectorPalmas {
 public:
  static constexpr int LIMIAR_MINIMO = 700;
  static constexpr uint32_t INTERVALO_MINIMO_PALMAS = 80;
  static constexpr uint32_t INTERVALO_MAXIMO_PALMAS = 600;
  struct Resultado {
    bool inicio = false;
    bool aceito = false;
    bool rejeitado = false;
    int acao = 0;
    uint32_t duracao = 0;
  };

  Resultado processar(int vpp, uint32_t agora) {
    Resultado r;
    if (!iniciado) { iniciado = true; inicioDetector = agora; }
    if (!pulso && contagem && agora - ultimaPalma > INTERVALO_MAXIMO_PALMAS) {
      if (contagem <= 2) r.acao = contagem;
      contagem = 0;
    }
    // Congela limiar entre a primeira e a segunda palma.
    if (!pulso && !contagem) limiar = limitar(fundo * 3, LIMIAR_MINIMO, 1000);
    const float silencio = limitar(fundo * 1.8f, 300, 450);
    if (pulso) {
      if (vpp > pico) pico = vpp;
      // Permite a cauda de uma palma forte decair sem exigir silêncio absoluto.
      float retorno = limitar(pico * 0.20f, silencio, 600);
      if (vpp < retorno) {
        if (!retornando) { retornando = true; inicioRetorno = agora; }
      } else {
        retornando = false;
      }
      if (retornando && agora - inicioRetorno >= 12 && agora - inicioRetorno <= 25) {
        pulso = false;
        r.aceito = true;
        r.duracao = agora - inicioPulso;
        ++contagem;
        ultimaPalma = inicioPulso;
        tevePalma = true;
        // A segunda palma já define a ação; não espera outra janela de 600 ms.
        if (contagem == 2) {
          r.acao = 2;
          contagem = 0;
        }
        quieto = false;
      } else if (agora - inicioPulso >= 120) {
        pulso = false;
        r.rejeitado = true;
        r.duracao = agora - inicioPulso;
        contagem = 0;
        quieto = false;
      }
    } else if (vpp < silencio) {
      ultimoSilencio = agora;
      if (!quieto) { quieto = true; inicioSilencio = agora; }
      if (agora - inicioSilencio >= 25 &&
          (!tevePalma || agora - ultimaPalma >= INTERVALO_MINIMO_PALMAS)) armado = true;
      // Só aprende em repouso estável. Fala/pulsos e pausas entre palmas
      // não aumentam o fundo nem tornam a segunda palma mais difícil.
      if (armado && !contagem && agora - inicioSilencio >= 300) {
        fundo += 0.005f * (vpp - fundo);
      }
    } else {
      if (armado && agora - inicioDetector >= 1000 &&
          agora - ultimoSilencio <= 25 && vpp >= limiar) {
        pulso = true;
        retornando = false;
        inicioPulso = agora;
        pico = vpp;
        r.inicio = true;
      }
      if (pulso || agora - ultimoSilencio > 25) armado = false;
      quieto = false;
    }
    return r;
  }

  // Descarta sequências se faltaram amostras; não inventa silêncio no buraco.
  void interromper() {
    pulso = armado = quieto = retornando = false;
    contagem = 0;
  }
  float fundo = 150;
  float limiar = LIMIAR_MINIMO;
  bool armado = false;
  bool pulso = false;
  int contagem = 0;
  int pico = 0;
 private:
  static float limitar(float x, float minimo, float maximo) {
    return x < minimo ? minimo : (x > maximo ? maximo : x);
  }
  bool iniciado = false, quieto = false, retornando = false;
  bool tevePalma = false;
  uint32_t inicioDetector = 0, inicioSilencio = 0, ultimoSilencio = 0;
  uint32_t inicioPulso = 0, inicioRetorno = 0, ultimaPalma = 0;
};
