#include "detector_palmas.h"
#include <initializer_list>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct Cenario {
  DetectorPalmas d;
  uint32_t tempo = 0;
  int simples = 0, duplas = 0, aceitos = 0, rejeitados = 0;
  void som(int vpp, int ms) {
    for (int i = 0; i < ms; i += 5) {
      auto r = d.processar(vpp, tempo);
      tempo += 5;
      simples += r.acao == 1;
      duplas += r.acao == 2;
      aceitos += r.aceito;
      rejeitados += r.rejeitado;
    }
  }
  void preparar() { som(80, 1300); }
  void palma(int pico = 1500) {
    som(pico, 10);
    som(500, 15);
    som(80, 35);
  }
};
int main() {
  { Cenario c; c.preparar(); c.som(80, 3000); assert(!c.aceitos); }
  { Cenario c; c.preparar(); c.palma(740); c.som(80, 700);
    assert(c.simples == 1 && c.aceitos == 1); }
  for (int intervalo : {85, 100, 120, 150, 200, 300, 400, 550, 600}) {
    Cenario c; c.preparar(); c.palma(4095); c.som(80, intervalo - 60);
    float limiar = c.d.limiar;
    c.palma(740); assert(c.d.limiar == limiar);
    assert(c.duplas == 1 && c.simples == 0); // Sem espera após a segunda palma.
    c.som(80, 700); assert(c.duplas == 1 && c.simples == 0);
  }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 40); c.palma();
    c.som(80, 10); c.palma(); // Eco após a segunda palma não vira comando.
    c.som(80, 700); assert(c.duplas == 1 && c.simples == 0); }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 650); c.palma();
    c.som(80, 700); assert(c.simples == 2 && !c.duplas); }
  { Cenario c; c.preparar(); c.som(1600, 1000); c.som(80, 1000);
    assert(c.rejeitados == 1 && c.aceitos == 0 && c.simples == 0); }
  { Cenario c; c.preparar(); for (int v = 100; v < 1600; v += 50) c.som(v, 10);
    c.som(80, 1000); assert(!c.aceitos); }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 10); c.palma();
    c.som(80, 700); assert(c.simples == 1 && !c.duplas); }
  { Cenario c; c.preparar(); c.palma(); c.d.interromper(); c.som(80, 1000);
    assert(!c.simples && !c.duplas); }
  { Cenario c; c.tempo = UINT32_MAX - 1500; c.preparar(); c.palma();
    c.som(80, 200); c.palma(); c.som(80, 700); assert(c.duplas == 1); }
  { Cenario c; c.preparar(); for (int i=0; i<10; ++i) {
      c.som(600, 400); c.som(80, 100);
    }
    assert(c.d.limiar <= 1000 && !c.aceitos); }
  puts("Detector: silencio, palmas simples/duplas, ecos, ruido prolongado, subida lenta, lacunas e overflow OK");
}
