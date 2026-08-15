import socket
import time

IP = "192.168.2.100"

def test_tcp():
    print(f"[*] Conectando TCP a {IP}:5001...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((IP, 5001))
        print("[+] Conectado. Enviando trafico al maximo...")
        print("[+] Mira el Monitor Serial de Arduino para ver los Mbps.")
        print("[+] Presiona Ctrl+C en esta ventana para parar.")
        data = b'A' * 2048
        while True:
            s.sendall(data)
    except KeyboardInterrupt:
        print("\n[!] Prueba detenida.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        s.close()

def test_udp():
    print(f"[*] Enviando UDP a {IP}:5002...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print("[+] Enviando trafico al maximo...")
        print("[+] Mira el Monitor Serial de Arduino para ver los Mbps.")
        print("[+] Presiona Ctrl+C en esta ventana para parar.")
        data = b'B' * 1024 
        while True:
            s.sendto(data, (IP, 5002))
    except KeyboardInterrupt:
        print("\n[!] Prueba detenida.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        s.close()

def test_modbus():
    print(f"[*] Probando Modbus TCP en {IP}:502...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((IP, 502))
        print("[+] Conectado. Bombardeando peticiones Modbus...")
        print("[+] Mira el Monitor Serial de Arduino para ver las Transacciones/seg.")
        print("[+] Presiona Ctrl+C en esta ventana para parar.")
        # Peticion tipica Modbus Read Holding Registers (12 bytes)
        req = b'\x00\x01\x00\x00\x00\x06\x01\x03\x00\x00\x00\x01'
        while True:
            s.sendall(req)
            resp = s.recv(9)
    except KeyboardInterrupt:
        print("\n[!] Prueba detenida.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        s.close()

if __name__ == "__main__":
    print("=== JWPLC ETHERNET BENCHMARK CLIENT ===")
    print("1. Inyectar trafico TCP (Puerto 5001)")
    print("2. Inyectar trafico UDP (Puerto 5002)")
    print("3. Inyectar peticiones Modbus TCP (Puerto 502)")
    sel = input("Selecciona la prueba a inyectar (1, 2 o 3): ")
    
    if sel == '1':
        test_tcp()
    elif sel == '2':
        test_udp()
    elif sel == '3':
        test_modbus()
    else:
        print("Opcion no valida.")
