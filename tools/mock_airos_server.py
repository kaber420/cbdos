#!/usr/bin/env python3
"""
mock_airos_server.py
-------------------
Servidor simulador de Ubiquiti AirOS (AirMAX v7/v8) para pruebas locales de CBDos.
Replica con fidelidad:
  1. POST /api/auth          -> Valida credenciales, emite X-CSRF-ID y Set-Cookie
  2. GET  /status.cgi        -> Requiere sesión/CSRF, entrega JSON con AP + N clientes CPE
  3. POST /api/auth/logout   -> Cierra la sesión
Soporta HTTP y HTTPS (autofirmado) para validar el cliente en ESP32-P4 y ESP32-S3.
"""

import sys
import os
import json
import uuid
import argparse
import ssl
import subprocess
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs

ACTIVE_SESSIONS = {}

def generate_mock_status(client_count=40):
    stations = []
    for i in range(1, client_count + 1):
        mac = f"04:18:D6:{i//256:02X}:{(i%256):02X}:{(i*7)%256:02X}"
        sig = -58 - (i % 20)
        noise = -96 - (i % 4)
        stations.append({
            "mac": mac,
            "name": f"CPE-Cliente-{i:02d}",
            "lastip": f"192.168.1.{100 + (i % 150)}",
            "signal": sig,
            "noisefloor": noise,
            "distance": 800 + (i * 75),
            "rx_chainmask": 3,
            "tx_chainmask": 3,
            "chains": [sig - 1, sig + 1],
            "rx": {
                "rate": "144.4 Mbps",
                "packets": 120000 + (i * 1234),
                "bytes": 50000000 + (i * 987654)
            },
            "tx": {
                "rate": "144.4 Mbps",
                "packets": 110000 + (i * 987),
                "bytes": 35000000 + (i * 654321)
            },
            "ccq": 970 + (i % 30),
            "airmax": {
                "quality": 90 + (i % 10),
                "capacity": 85 + (i % 15),
                "priority": 2
            },
            "lan": {
                "status": "100Mbps-Full" if (i % 10 != 0) else "10Mbps-Half",
                "cable_len": 15 + (i % 25)
            }
        })

    status_data = {
        "host": {
            "hostname": "AP-Sector-Norte",
            "uptime": 1254320,
            "netrole": "bridge",
            "fwversion": "WA.v8.7.1",
            "devmodel": "Rocket 5AC Prism",
            "loadavg": [0.15, 0.22, 0.18],
            "totalram": 268435456,
            "freeram": 184549376
        },
        "wireless": {
            "mode": "ap",
            "essid": "CBD-SectorNorte-5G",
            "frequency": 5745,
            "chanbw": 20,
            "antenna": "AirMAX Sector 120 19dBi",
            "count": client_count,
            "hide_essid": 0
        },
        "interfaces": [
            {"ifname": "eth0", "status": "up", "speed": 1000, "duplex": "full", "rx_bytes": 845920392, "tx_bytes": 194830201},
            {"ifname": "ath0", "status": "up", "speed": 300, "duplex": "full", "rx_bytes": 194830201, "tx_bytes": 845920392}
        ],
        "sta": stations
    }
    return status_data

class AirOsMockHandler(BaseHTTPRequestHandler):
    client_count = 40
    valid_user = "ubnt"
    valid_pass = "ubnt123"

    def _send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "X-CSRF-ID, Content-Type, Cookie")

    def do_OPTIONS(self):
        self.send_response(200)
        self._send_cors_headers()
        self.end_headers()

    def do_POST(self):
        path = self.path.split("?")[0]
        content_len = int(self.headers.get("Content-Length", 0))
        post_body = self.rfile.read(content_len).decode("utf-8", errors="ignore")

        if path == "/api/auth":
            # Autenticación AirOS
            params = parse_qs(post_body)
            user = params.get("username", [""])[0]
            pwd = params.get("password", [""])[0]

            if user == self.valid_user and pwd == self.valid_pass:
                session_id = f"AIROS_{uuid.uuid4().hex[:16]}"
                csrf_token = f"CSRF_{uuid.uuid4().hex[:24]}"
                ACTIVE_SESSIONS[session_id] = csrf_token

                print(f"[AUTH OK] Usuario '{user}' autenticado. Nueva Sesión: {session_id}, CSRF: {csrf_token}")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("X-CSRF-ID", csrf_token)
                self.send_header("Set-Cookie", f"AIROS_SESSIONID={session_id}; Path=/; HttpOnly")
                self._send_cors_headers()
                self.end_headers()
                resp = json.dumps({"status": "success", "message": "Authenticated"}).encode("utf-8")
                self.wfile.write(resp)
            else:
                print(f"[AUTH FAIL] Credenciales invalidas para usuario '{user}'")
                self.send_response(401)
                self.send_header("Content-Type", "application/json")
                self._send_cors_headers()
                self.end_headers()
                resp = json.dumps({"status": "error", "message": "Invalid credentials"}).encode("utf-8")
                self.wfile.write(resp)

        elif path == "/api/auth/logout":
            print("[LOGOUT] Sesion finalizada.")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(b'{"status":"logged_out"}')

        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        path = self.path.split("?")[0]

        if path == "/status.cgi":
            # Verificar headers requeridos
            csrf_hdr = self.headers.get("X-CSRF-ID", "")
            cookie_hdr = self.headers.get("Cookie", "")

            session_id = None
            if "AIROS_SESSIONID=" in cookie_hdr:
                for part in cookie_hdr.split(";"):
                    part = part.strip()
                    if part.startswith("AIROS_SESSIONID="):
                        session_id = part.split("=", 1)[1]
                        break

            # En modo laxo de laboratorio, si no hay sesiones previas aceptamos la petición con advertencia
            # pero validamos si session_id coincide con el csrf registrado
            authorized = False
            if session_id and session_id in ACTIVE_SESSIONS:
                if ACTIVE_SESSIONS[session_id] == csrf_hdr or not csrf_hdr:
                    authorized = True

            # Si no hay sesiones activas, aceptar para facilitar prueba directa o curl
            if not ACTIVE_SESSIONS:
                authorized = True

            if not authorized:
                print(f"[STATUS REJECT] Peticion no autorizada (Cookie: {cookie_hdr}, CSRF: {csrf_hdr})")
                self.send_response(403)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"error": "Unauthorized / Missing CSRF or Session"}')
                return

            status_obj = generate_mock_status(self.client_count)
            body_bytes = json.dumps(status_obj, indent=2).encode("utf-8")

            print(f"[STATUS OK] Entregando telemetria: {len(status_obj['sta'])} clientes, {len(body_bytes)} bytes (~{round(len(body_bytes)/1024, 2)} KB)")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body_bytes)))
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(body_bytes)

        elif path == "/" or path == "/index.html":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<h1>CBDos Mock Ubiquiti AirOS Server</h1><p>Status: Ready</p>")

        else:
            self.send_response(404)
            self.end_headers()

def generate_self_signed_cert(cert_file, key_file):
    """Genera certificado autofirmado con OpenSSL si no existe."""
    if os.path.exists(cert_file) and os.path.exists(key_file):
        return
    print(f"[CERT] Generando certificado autofirmado para HTTPS: {cert_file} ...")
    cmd = [
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-keyout", key_file,
        "-out", cert_file, "-days", "365", "-nodes",
        "-subj", "/CN=ubnt-mock.local/O=CBDos/OU=Lab"
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def main():
    parser = argparse.ArgumentParser(description="Ubiquiti AirOS Mock Server")
    parser.add_argument("--port", type=int, default=8080, help="Puerto de escucha (default: 8080)")
    parser.add_argument("--https", action="store_true", help="Habilitar modo HTTPS")
    parser.add_argument("--clients", type=int, default=40, help="Cantidad de clientes CPE en /status.cgi (default: 40)")
    parser.add_argument("--user", type=str, default="ubnt", help="Usuario requerido (default: ubnt)")
    parser.add_argument("--passw", type=str, default="ubnt123", help="Contraseña requerida (default: ubnt123)")
    args = parser.parse_args()

    AirOsMockHandler.client_count = args.clients
    AirOsMockHandler.valid_user = args.user
    AirOsMockHandler.valid_pass = args.passw

    server_address = ("0.0.0.0", args.port)
    httpd = HTTPServer(server_address, AirOsMockHandler)

    protocol = "http"
    if args.https:
        cert_file = os.path.join(os.path.dirname(__file__), "mock_airos_cert.pem")
        key_file = os.path.join(os.path.dirname(__file__), "mock_airos_key.pem")
        generate_self_signed_cert(cert_file, key_file)

        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(certfile=cert_file, keyfile=key_file)
        httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
        protocol = "https"

    print("=" * 65)
    print(f"  Servidor Mock AirOS escuchando en {protocol}://0.0.0.0:{args.port}")
    print(f"  Estaciones CPE simuladas : {args.clients}")
    print(f"  Credenciales aceptadas   : {args.user} / {args.passw}")
    print(f"  Endpoints disponibles    :")
    print(f"    - POST {protocol}://<IP>:{args.port}/api/auth")
    print(f"    - GET  {protocol}://<IP>:{args.port}/status.cgi")
    print(f"    - POST {protocol}://<IP>:{args.port}/api/auth/logout")
    print("=" * 65)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[STOP] Servidor detenido por el usuario.")
        httpd.server_close()

if __name__ == "__main__":
    main()
