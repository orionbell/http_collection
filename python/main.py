import socket
import sys
from threading import Event, Thread
import queue


def print_hello():
    print("Hello World")
    return "<h1>Hello dad</h1>"


ARGS = sys.argv
HOSTNAME = socket.gethostname()
IP = ARGS[1] if len(ARGS) >= 2 else socket.gethostbyname(HOSTNAME)
PORT = int(ARGS[2]) if len(ARGS) >= 3 else 80
PATHS = {
    "/": {"GET": "index.html"},
    "/users": {"POST": print_hello},
}
NUM_OF_THREADS = int(ARGS[3]) if len(ARGS) >= 4 else 32
CLIENTS = queue.Queue()
WORKERS: list[Thread] = []
alive = Event()


def main():
    global alive
    # Initiating thread pool
    for _ in range(NUM_OF_THREADS):
        WORKERS.append(Thread(target=execute))
    for worker in WORKERS:
        worker.start()
    try:
        # Creating an IPv4 TCP socket and listening on the provided address and port
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        server.bind((IP, PORT))
        server.listen()
        print(f"[*] Server is listening on {IP}:{PORT}")
    except Exception as e:
        print(f"[!] Error: {e}", file=sys.stderr)
        exit(1)
    while True:
        try:
            client = server.accept()
            print(f"[+] New request from {client[1][0]}:{client[1][1]}")
            client_socket = client[0]
            CLIENTS.put(client_socket)
        except KeyboardInterrupt:
            print("[!] SIGKILL detected, exiting ...")
            alive.set()
            i = 1
            for worker in WORKERS:
                worker.join()
                i += 1

            server.close()
            return
        except Exception as e:
            print(f"[!] Error:s {e}", file=sys.stderr)
            exit(1)


def execute():
    while not alive.is_set():
        try:
            client = CLIENTS.get(timeout=1)
            if client:
                handle_client(client)
        except queue.Empty:
            continue


def read_request(client_socket):
    data = b""
    while True:
        chunk = client_socket.recv(4096)
        data += chunk
        if b"\r\n\r\n" in chunk:
            break

    return data.decode()


def handle_client(client_socket: socket.socket):
    req = read_request(client_socket)
    method, path, _ = parse_request(req)
    print(f"[*] Resource requested: {method} {path}")
    res = "".encode()
    try:
        html = ""
        action = PATHS[path][method]
        if isinstance(action, str):
            with open(f"../public/{action}", "r") as f:
                html = f.read()
                res = f"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: {len(str(html))}\r\n\r\n{html}".encode()
        elif callable(action):
            html = action()
            if html:
                res = f"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: {len(str(html))}\r\n\r\n{html}".encode()
            else:
                res = "HTTP/1.1 200 OK\r\n".encode()
        else:
            raise KeyError
    except KeyError:
        res = "HTTP/1.1 404 Not Found\r\n".encode()
    client_socket.send(res)
    client_socket.close()


def parse_request(req: str) -> tuple:
    first_line = req.split("\r\n")[0]
    method, path, proto = first_line.split(" ")
    return method, path, proto


if __name__ == "__main__":
    main()
