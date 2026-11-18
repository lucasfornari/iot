#pragma once

// Não coloque credenciais aqui. Os valores vêm do arquivo .env na raiz do
// projeto (veja .env.exemplo), injetados por scripts/carregar_env.py.
// Os padrões vazios só permitem compilar sem .env; o firmware avisa no
// OLED ("CFG") e na serial.

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_SENHA
#define WIFI_SENHA ""
#endif
#ifndef SINRIC_APP_KEY
#define SINRIC_APP_KEY ""
#endif
#ifndef SINRIC_APP_SECRET
#define SINRIC_APP_SECRET ""
#endif
#ifndef SINRIC_DEVICE_ID
#define SINRIC_DEVICE_ID ""
#endif
