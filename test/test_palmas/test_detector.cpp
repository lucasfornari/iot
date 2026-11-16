#include "detector_palmas.h"
#include "analise_microfone.h"
#include <initializer_list>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

struct Cenario {
  DetectorPalmas d;
  uint32_t tempo = 0;
  int simples = 0, duplas = 0, aceitos = 0, rejeitados = 0, tonais = 0;
  void som(int vpp, int ms, int textura = 1500) {
    for (int i = 0; i < ms; i += 5) {
      auto r = d.processar(vpp, textura, tempo);
      tempo += 5;
      simples += r.acao == 1;
      duplas += r.acao == 2;
      aceitos += r.aceito;
      rejeitados += r.rejeitado;
      tonais += r.motivo == DetectorPalmas::Resultado::SOM_TONAL;
    }
  }
  void preparar(int fundo = 80) { som(fundo, 1300); }
  void palma(int pico = 3300, int fundo = 80) {
    som(pico, 10);
    som(500 > fundo ? 500 : fundo, 15);
    som(fundo, 35);
  }
};

// Janelas de 32 amostras a 8 kHz para testar a extração do sinal.
AnaliseMicrofone onda(int amplitude, int frequencia, int centro = 2048) {
  AnaliseMicrofone a;
  for (int i = 0; i < 32; ++i) {
    a.adicionar(centro + int(amplitude * sin(6.28318530718 * frequencia * i / 8000)));
  }
  return a;
}

int main() {
  { AnaliseMicrofone a; assert(a.vpp() == 0 && a.textura() == 0 && !a.saturada());
    for (int i = 0; i < 32; ++i) a.adicionar(2048);
    assert(a.vpp() == 0 && a.textura() == 0 && !a.saturada()); }
  { auto a = onda(900, 250); auto b = onda(900, 250, 1300);
    assert(a.vpp() == b.vpp() && a.textura() == b.textura());
    assert(a.textura() < DetectorPalmas::TEXTURA_MINIMA); }
  { AnaliseMicrofone a;
    for (int i = 0; i < 32; ++i) a.adicionar(i % 2 ? 4095 : 0);
    assert(a.saturada() && a.vpp() == 4095 && a.textura() > 3000); }
  { Cenario c; c.preparar(); c.som(80, 3000); assert(!c.aceitos); }
  { Cenario c; c.preparar(); c.palma(740); c.som(80, 700);
    assert(c.simples == 0 && c.aceitos == 0 && c.rejeitados == 1); }
  for (int intervalo : {85, 100, 120, 150, 200, 300, 400, 550, 600}) {
    Cenario c; c.preparar(); c.palma(4095); c.som(80, intervalo - 60);
    float limiar = c.d.limiar;
    c.palma(3000); assert(c.d.limiar == limiar);
    assert(c.duplas == 1 && c.simples == 0); // Sem espera após a segunda palma.
    c.som(80, 700); assert(c.duplas == 1 && c.simples == 0);
  }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 40); c.palma();
    c.som(80, 10); c.palma(); // Eco após a segunda palma não vira comando.
    c.som(80, 700); assert(c.duplas == 1 && c.simples == 0); }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 650); c.palma();
    c.som(80, 700); assert(c.simples == 2 && !c.duplas); }
  { Cenario c; c.preparar(); c.som(1600, 1000); c.som(80, 1000);
    assert(c.rejeitados == 1 && c.aceitos == 0 && c.simples == 0);
    c.palma(); c.som(80, 700); assert(c.simples == 1); }
  { Cenario c; c.preparar(); for (int v = 100; v < 1600; v += 50) c.som(v, 10);
    c.som(80, 1000); assert(!c.aceitos); }
  { Cenario c; c.preparar(); c.palma(); c.som(80, 10); c.palma();
    c.som(80, 700); assert(c.simples == 1 && !c.duplas); }
  { Cenario c; c.preparar(); c.palma(); c.d.interromper(); c.som(80, 1000);
    assert(!c.simples && !c.duplas); }
  { Cenario c; c.tempo = UINT32_MAX - 1500; c.preparar(); c.palma();
    c.som(80, 200); c.palma(); c.som(80, 700); assert(c.duplas == 1); }
  // Antes: um fundo acima de 450 impedia rearmar indefinidamente.
  for (int fundo : {500, 800, 1100}) {
    Cenario c; c.preparar(fundo); c.som(fundo, 3000);
    assert(c.d.armado && !c.aceitos);
    c.palma(4095, fundo); c.som(fundo, 200); c.palma(4095, fundo);
    assert(c.duplas == 1);
    c.som(fundo, 1000); c.palma(4095, fundo); c.som(fundo, 700);
    assert(c.simples == 1); // Continua aceitando com o OLED ainda ativo.
  }
  // Antes: uma sílaba curta com o mesmo envelope de uma palma era aceita.
  { Cenario c; c.preparar(); auto voz = onda(900, 250);
    for (int i = 0; i < 10; ++i) {
      c.som(voz.vpp(), 30, voz.textura()); c.som(80, 150);
    }
    assert(c.tonais == 10 && !c.aceitos && !c.simples && !c.duplas);
    c.palma(); c.som(80, 700); assert(c.simples == 1); }
  // Ruído agudo prolongado também é rejeitado, apesar da textura.
  { Cenario c; c.preparar(); c.som(2000, 100, 1800); c.som(80, 700);
    assert(c.rejeitados == 1 && !c.aceitos); }
  // Uma palma saturada com cauda forte não bloqueia o próximo comando.
  { Cenario c; c.preparar(); c.som(4095, 10); c.som(900, 20); c.som(80, 170);
    c.palma(3000); assert(c.duplas == 1); }
  // Caudas medidas no MAX4466: repiques na queda não devem perder palmas fortes.
  { Cenario c; c.preparar();
    for (int v : {1029, 4095, 4095, 2484, 1713, 1125, 1856, 1119,
                  1868, 1228, 1711, 1549, 1083, 847, 1103, 1345,
                  1320, 1039, 903, 1293, 995, 839, 611, 593, 617}) c.som(v, 5);
    c.som(80, 275);
    for (int v : {4095, 4095, 4095, 2516, 1638, 1487, 1567, 1145,
                  1614, 1625, 1847, 1631, 1301, 2226, 1211, 1080,
                  1059, 1658, 1107, 1379, 1574, 1067}) c.som(v, 5);
    c.som(80, 700); assert(c.duplas == 1 && c.aceitos == 2); }
  // Um eco tardio logo após a dupla não inicia outra sequência.
  { Cenario c; c.preparar(); c.palma(); c.som(80, 140); c.palma();
    c.som(80, 40); c.palma(); c.som(80, 1000);
    assert(c.duplas == 1 && !c.simples && c.aceitos == 2); }
  // O timeout tem prioridade: uma queda muito tardia não valida o pulso.
  { Cenario c; c.preparar(); c.som(1600, 110); c.som(80, 700);
    assert(c.rejeitados == 1 && !c.aceitos); }
  // Ruídos reais do fim da coleta: textura alta sozinha não confirma palma.
  { Cenario c; c.preparar(); c.som(752, 10, 570); c.som(80, 195);
    c.som(1150, 10, 743); c.som(80, 700);
    assert(!c.aceitos && !c.simples && !c.duplas); }
  { Cenario c; c.preparar(); c.palma(2500); c.som(80, 700);
    assert(!c.aceitos && c.rejeitados == 1); }
  { Cenario c; c.preparar(); c.som(3008, 10, 788); c.som(80, 700);
    assert(c.tonais == 1 && !c.aceitos); }
  // A fala após uma palma válida não pode apagar o comando pendente.
  { Cenario c; c.preparar(); c.palma(); c.som(80, 140);
    c.som(1800, 20, 200); c.som(80, 700);
    assert(c.simples == 1 && c.aceitos == 1 && c.tonais == 1); }
  // Rajada real de ruído: um candidato rejeitado não deve rearmar de imediato.
  { Cenario c; c.preparar(); c.som(2549, 10, 1042); c.som(80, 50);
    c.som(4005, 10, 1663); c.som(80, 700);
    assert(!c.aceitos && !c.simples && !c.duplas);
    c.palma(); c.som(80, 700); assert(c.simples == 1); }
  puts("Detector: simples/duplas, voz sintetica, fundo elevado, saturacao, ecos, recuperacao, lacunas e overflow OK");
}
