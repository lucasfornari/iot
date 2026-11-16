#pragma once
#include <stdint.h>

// Combina contraste de volume, ataque, textura e decaimento do som.
// Vpp em unidades do ADC, textura em milésimos e tempos em ms.
class DetectorPalmas {
 public:
  static constexpr int LIMIAR_MINIMO = 700;
  // O limiar de ataque pode pegar só o começo da palma. A confirmação
  // exige um pico maior, calibrado para palmas próximas neste MAX4466.
  static constexpr int PICO_MINIMO = 2800;
  static constexpr int TEXTURA_MINIMA = 800;
  static constexpr uint32_t REARME_APOS_RUIDO = 80;
  static constexpr uint32_t INTERVALO_MINIMO_PALMAS = 80;
  static constexpr uint32_t INTERVALO_MAXIMO_PALMAS = 600;
  struct Resultado {
    enum Motivo { NENHUM, SOM_LONGO, SOM_TONAL, SOM_FRACO } motivo = NENHUM;
    bool inicio = false;
    bool aceito = false;
    bool rejeitado = false;
    int acao = 0;
    uint32_t duracao = 0;
  };

  Resultado processar(int vpp, int textura, uint32_t agora) {
    Resultado r;
    if (!iniciado) { iniciado = true; inicioDetector = agora; }
    if (!pulso && contagem && agora - ultimaPalma > INTERVALO_MAXIMO_PALMAS) {
      if (contagem <= 2) r.acao = contagem;
      contagem = 0;
    }
    // O fundo acompanha também ambientes acima do antigo teto de silêncio.
    // Pulsos e a pausa entre duas palmas não contaminam a estimativa.
    if (!pulso && !contagem) limiar = limitar(fundo * 2.5f, LIMIAR_MINIMO, 3500);
    const float silencio = limitar(fundo * 1.5f + 50, 250, limiar * 0.65f);
    const bool bloqueado = (teveDupla && agora - ultimaDupla < 180) ||
                           (teveRejeicao && agora - ultimaRejeicao < REARME_APOS_RUIDO);
    if (pulso) {
      if (vpp > pico) pico = vpp;
      // Avalia a textura no ataque, sem aproveitar ruído depois do impacto.
      if (agora - inicioPulso <= 12 && vpp >= limiar && vpp >= pico * 0.5f &&
          textura > texturaPico) texturaPico = textura;
      // Uma palma saturada pode ter uma cauda bem acima de 600 no ADC.
      float retorno = limitar(pico * 0.40f, silencio, 4095);
      if (vpp < retorno) {
        if (!retornando) { retornando = true; inicioRetorno = agora; }
      } else {
        retornando = false;
      }
      if ((!retornando && agora - inicioPulso >= 70) ||
          (retornando && inicioRetorno - inicioPulso >= 70) ||
          agora - inicioPulso >= 100) {
        pulso = false;
        r.rejeitado = true;
        r.motivo = Resultado::SOM_LONGO;
        r.duracao = agora - inicioPulso;
        quieto = false;
      } else if (retornando && agora - inicioRetorno >= 15) {
        pulso = false;
        r.duracao = agora - inicioPulso;
        if (texturaPico >= TEXTURA_MINIMA && pico >= PICO_MINIMO) {
          r.aceito = true;
          ++contagem;
          ultimaPalma = inicioPulso;
          tevePalma = true;
          // A segunda palma já define a ação, inclusive com uma tela ativa.
          if (contagem == 2) {
            r.acao = 2;
            contagem = 0;
            teveDupla = true;
            ultimaDupla = agora;
          }
        } else {
          r.rejeitado = true;
          r.motivo = texturaPico < TEXTURA_MINIMA ? Resultado::SOM_TONAL : Resultado::SOM_FRACO;
        }
        quieto = false;
      }
    } else if (vpp < silencio) {
      ultimoSilencio = agora;
      if (!quieto) { quieto = true; inicioSilencio = agora; }
      if (!bloqueado && agora - inicioSilencio >= 20 &&
          (!tevePalma || agora - ultimaPalma >= INTERVALO_MINIMO_PALMAS)) armado = true;
    } else {
      if (armado && !bloqueado && agora - inicioDetector >= 1000 &&
          agora - ultimoSilencio <= 15 && vpp >= limiar &&
          vpp >= anterior * 1.8f) {
        pulso = true;
        retornando = false;
        inicioPulso = agora;
        pico = vpp;
        texturaPico = textura;
        r.inicio = true;
      }
      if (pulso || agora - ultimoSilencio > 15) armado = false;
      quieto = false;
    }
    if (!pulso && !contagem && !r.aceito && !r.rejeitado && !bloqueado) {
      // Aprende inclusive ruído contínuo antes de rearmar. O teto antigo
      // impedia qualquer nova palma se o ambiente ficasse acima de 450.
      fundo += (vpp > fundo ? 0.025f : 0.06f) * (vpp - fundo);
    }
    if (r.rejeitado) {
      teveRejeicao = true;
      ultimaRejeicao = agora;
      armado = false;
      // Um som inválido não apaga uma palma já confirmada.
    }
    anterior = vpp;
    return r;
  }

  // Descarta sequências se faltaram amostras; não inventa silêncio no buraco.
  void interromper() {
    pulso = armado = quieto = retornando = false;
    contagem = 0;
    anterior = 4095; // A primeira janela após a lacuna não define um ataque.
  }
  float fundo = 150;
  float limiar = LIMIAR_MINIMO;
  bool armado = false;
  bool pulso = false;
  int contagem = 0;
  int pico = 0;
  int texturaPico = 0;
 private:
  static float limitar(float x, float minimo, float maximo) {
    return x < minimo ? minimo : (x > maximo ? maximo : x);
  }
  bool iniciado = false, quieto = false, retornando = false;
  bool tevePalma = false;
  bool teveDupla = false;
  bool teveRejeicao = false;
  int anterior = 4095;
  uint32_t inicioDetector = 0, inicioSilencio = 0, ultimoSilencio = 0;
  uint32_t inicioPulso = 0, inicioRetorno = 0, ultimaPalma = 0;
  uint32_t ultimaDupla = 0;
  uint32_t ultimaRejeicao = 0;
};
