# Lê o arquivo .env da raiz do projeto e repassa as credenciais ao firmware
# como macros de compilação (-DWIFI_SSID="..."). O ESP32 não lê .env em
# tempo de execução: os valores entram no binário durante o `pio run`.
# Variáveis de ambiente do sistema com o mesmo nome têm prioridade.
import os

Import("env")  # noqa: F821 (injetado pelo PlatformIO/SCons)

CHAVES = [
    "WIFI_SSID",
    "WIFI_SENHA",
    "SINRIC_APP_KEY",
    "SINRIC_APP_SECRET",
    "SINRIC_DEVICE_ID",
]


def ler_env(caminho):
    valores = {}
    with open(caminho, encoding="utf-8") as arquivo:
        for linha in arquivo:
            linha = linha.strip()
            if not linha or linha.startswith("#") or "=" not in linha:
                continue
            chave, valor = linha.split("=", 1)
            chave = chave.strip()
            valor = valor.strip()
            if len(valor) >= 2 and valor[0] == valor[-1] and valor[0] in "\"'":
                valor = valor[1:-1]
            valores[chave] = valor
    return valores


caminho_env = os.path.join(env.subst("$PROJECT_DIR"), ".env")  # noqa: F821
valores = {}
if os.path.isfile(caminho_env):
    valores = ler_env(caminho_env)
else:
    print("[.env] Arquivo .env nao encontrado; copie .env.exemplo para .env")

faltando = []
definicoes = []
for chave in CHAVES:
    valor = os.environ.get(chave) or valores.get(chave, "")
    if not valor and chave != "WIFI_SENHA":
        faltando.append(chave)
    definicoes.append((chave, env.StringifyMacro(valor)))  # noqa: F821

env.Append(CPPDEFINES=definicoes)  # noqa: F821
if faltando:
    print("[.env] Aviso: valores vazios: " + ", ".join(faltando))
