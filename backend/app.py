#!/usr/bin/env python3
"""
Servidor local com SQLite para armazenamento e histórico de telemetria IoT.
Compatível com Python 3 padrão (sem dependências obrigatórias).
"""

import os
import sys
import json
import sqlite3
import datetime
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

PORTA_HTTP = 5000
MQTT_HOST = os.environ.get("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(os.environ.get("MQTT_PORT", 1883))

DIRETORIO_ATUAL = os.path.dirname(os.path.abspath(__file__))
DIRETORIO_RAIZ = os.path.dirname(DIRETORIO_ATUAL)
CAMINHO_DB = os.path.join(DIRETORIO_ATUAL, "dados.db")
CAMINHO_HTML = os.path.join(DIRETORIO_RAIZ, "data")

def obter_conexao():
    conn = sqlite3.connect(CAMINHO_DB)
    conn.row_factory = sqlite3.Row
    return conn

def inicializar_banco():
    with obter_conexao() as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS leituras (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                data_hora DATETIME DEFAULT CURRENT_TIMESTAMP,
                temperatura REAL NOT NULL,
                umidade REAL NOT NULL
            );
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_leituras_data ON leituras(data_hora);")
        conn.commit()
    print(f"[SQLite] Banco de dados inicializado em: {CAMINHO_DB}")

def salvar_leitura(temperatura, umidade, data_hora=None):
    if data_hora is None:
        data_hora = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with obter_conexao() as conn:
        conn.execute(
            "INSERT INTO leituras (data_hora, temperatura, umidade) VALUES (?, ?, ?)",
            (data_hora, float(temperatura), float(umidade))
        )
        conn.commit()

def consultar_historico(periodo="dia"):
    with obter_conexao() as conn:
        cursor = conn.cursor()
        
        if periodo == "tempo_real":
            cursor.execute("""
                SELECT strftime('%H:%M:%S', data_hora) as label,
                       temperatura,
                       umidade
                FROM leituras
                ORDER BY id DESC
                LIMIT 60
            """)
            linhas = cursor.fetchall()
            linhas = list(reversed(linhas))
            
        elif periodo == "dia":
            cursor.execute("""
                SELECT strftime('%H:%M', data_hora) as label,
                       ROUND(AVG(temperatura), 1) as temperatura,
                       ROUND(AVG(umidade), 1) as umidade
                FROM leituras
                WHERE data_hora >= datetime('now', '-1 day', 'localtime')
                GROUP BY strftime('%Y-%m-%d %H', data_hora), (strftime('%M', data_hora) / 10)
                ORDER BY data_hora ASC
            """)
            linhas = cursor.fetchall()
            
        elif periodo == "semana":
            cursor.execute("""
                SELECT strftime('%d/%m %H:00', data_hora) as label,
                       ROUND(AVG(temperatura), 1) as temperatura,
                       ROUND(AVG(umidade), 1) as umidade
                FROM leituras
                WHERE data_hora >= datetime('now', '-7 days', 'localtime')
                GROUP BY strftime('%Y-%m-%d %H', data_hora)
                ORDER BY data_hora ASC
            """)
            linhas = cursor.fetchall()
            
        elif periodo == "mes":
            cursor.execute("""
                SELECT strftime('%d/%m', data_hora) as label,
                       ROUND(AVG(temperatura), 1) as temperatura,
                       ROUND(AVG(umidade), 1) as umidade
                FROM leituras
                WHERE data_hora >= datetime('now', '-30 days', 'localtime')
                GROUP BY strftime('%Y-%m-%d', data_hora)
                ORDER BY data_hora ASC
            """)
            linhas = cursor.fetchall()
            
        else: # tudo
            cursor.execute("""
                SELECT strftime('%d/%m/%Y', data_hora) as label,
                       ROUND(AVG(temperatura), 1) as temperatura,
                       ROUND(AVG(umidade), 1) as umidade
                FROM leituras
                GROUP BY strftime('%Y-%m-%d', data_hora)
                ORDER BY data_hora ASC
            """)
            linhas = cursor.fetchall()
            
        return [dict(l) for l in linhas]

class RequisicaoHandler(BaseHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):
        url = urlparse(self.path)
        caminho = url.path
        
        if caminho in ["/", "/index.html"]:
            arquivo = os.path.join(CAMINHO_HTML, "index.html")
            if os.path.exists(arquivo):
                with open(arquivo, "rb") as f:
                    conteudo = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(conteudo)
                return
                
        elif caminho == "/mqtt.min.js":
            arquivo = os.path.join(CAMINHO_HTML, "mqtt.min.js")
            if os.path.exists(arquivo):
                with open(arquivo, "rb") as f:
                    conteudo = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "application/javascript")
                self.end_headers()
                self.wfile.write(conteudo)
                return

        elif caminho == "/api/leituras":
            params = parse_qs(url.query)
            periodo = params.get("periodo", ["dia"])[0]
            dados = consultar_historico(periodo)
            
            resposta = json.dumps({"periodo": periodo, "total": len(dados), "leituras": dados}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(resposta)
            return

        elif caminho == "/api/status":
            with obter_conexao() as conn:
                cursor = conn.cursor()
                cursor.execute("SELECT COUNT(*) FROM leituras")
                total = cursor.fetchone()[0]
                cursor.execute("SELECT * FROM leituras ORDER BY id DESC LIMIT 1")
                ultima = cursor.fetchone()
                
            resposta = json.dumps({
                "status": "online",
                "total_registros": total,
                "ultima_leitura": dict(ultima) if ultima else None
            }).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(resposta)
            return

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"Nao encontrado")

    def do_POST(self):
        url = urlparse(self.path)
        if url.path == "/api/leituras":
            comprimento = int(self.headers.get("Content-Length", 0))
            corpo = self.rfile.read(comprimento).decode("utf-8")
            try:
                dados = json.loads(corpo)
                temp = float(dados["temperatura"])
                umi = float(dados["umidade"])
                salvar_leitura(temp, umi)
                
                resposta = json.dumps({"sucesso": True}).encode("utf-8")
                self.send_response(201)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(resposta)
                return
            except Exception as e:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(json.dumps({"erro": str(e)}).encode("utf-8"))
                return

        self.send_response(404)
        self.end_headers()

def iniciar_cliente_mqtt():
    """Tenta conectar ao broker MQTT via paho-mqtt se instalado."""
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("[MQTT] paho-mqtt nao instalado. O salvamento no SQLite funcionara via API HTTP pelo navegador.")
        return

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print(f"[MQTT] Coletor conectado ao broker {MQTT_HOST}:{MQTT_PORT}")
            client.subscribe("sensor/temperatura")
            client.subscribe("sensor/umidade")
            client.subscribe("sensor/dados")
        else:
            print(f"[MQTT] Falha de conexao com codigo {rc}")

    estado = {"temperatura": None, "umidade": None}

    def on_message(client, userdata, msg):
        topico = msg.topic
        payload = msg.payload.decode("utf-8")
        try:
            if topico == "sensor/temperatura":
                estado["temperatura"] = float(payload)
            elif topico == "sensor/umidade":
                estado["umidade"] = float(payload)
            elif topico == "sensor/dados":
                d = json.loads(payload)
                if "temperatura" in d and "umidade" in d:
                    salvar_leitura(d["temperatura"], d["umidade"])
                    return

            if estado["temperatura"] is not None and estado["umidade"] is not None:
                salvar_leitura(estado["temperatura"], estado["umidade"])
                estado["temperatura"] = None
                estado["umidade"] = None
        except Exception as e:
            print(f"[MQTT] Erro ao processar mensagem: {e}")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(MQTT_HOST, MQTT_PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"[MQTT] Nao foi possivel conectar ao broker {MQTT_HOST}:{MQTT_PORT} ({e}).")

if __name__ == "__main__":
    inicializar_banco()
    
    # Thread do coletor MQTT (se paho disponivel)
    t = threading.Thread(target=iniciar_cliente_mqtt, daemon=True)
    t.start()

    servidor = HTTPServer(("0.0.0.0", PORTA_HTTP), RequisicaoHandler)
    print(f"[HTTP] Painel e API rodando em http://localhost:{PORTA_HTTP}/")
    print(f"[HTTP] Pressione Ctrl+C para encerrar.")
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor encerrado.")
