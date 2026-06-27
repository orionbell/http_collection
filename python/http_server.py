from threading import Thread, Event
import queue
import socket
import sys


class HTTPServer:
    def __init__(self, use_cli_args=False):
        self.__addr = socket.gethostbyname(socket.gethostname())
        self.__port = 80
        self.__threads = 32
        if len(sys.argv) >= 2 and use_cli_args:
            self.__addr = sys.argv[1]
            if len(sys.argv) >= 3:
                try:
                    self.__port = int(sys.argv[2])
                except ValueError:
                    self.__port = 80
                    print("[!] Error: Invalid value for port, using the default (80).")
                if len(sys.argv) >= 4:
                    try:
                        self.__threads = int(sys.argv[3])
                    except ValueError:
                        self.__threads = 32
                        print(
                            "[!] Error: Invalid value for threads, using the default (32)."
                        )
        self.__clients = queue.Queue()
        self.__workers = []
        self.__alive = Event()
        self.__routes = {}
        for _ in range(self.__threads):
            self.__workers.append(Thread(target=self.__execute))
        for worker in self.__workers:
            worker.start()

    def listen(self):
        try:
            # Creating an IPv4 TCP socket and listening on the provided address and port
            server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            server.bind((self.__addr, self.__port))
            server.listen()
            print(f"[*] Server is listening on {self.__addr}:{self.__port}")
        except Exception as e:
            try:
                print(f"[!] Error: {e}", file=sys.stderr)
                exit(1)
            except KeyboardInterrupt:
                exit(1)
        while True:
            try:
                client = server.accept()
                print(f"[+] New request from {client[1][0]}:{client[1][1]}")
                client_socket = client[0]
                self.__clients.put(client_socket)
            except KeyboardInterrupt:
                print("[!] SIGKILL detected, exiting ...")
                self.__alive.set()
                i = 1
                for worker in self.__workers:
                    worker.join()
                    i += 1

                server.close()
                return
            except Exception as e:
                print(f"[!] Error:s {e}", file=sys.stderr)
                exit(1)

    def route(self, path, method, action):
        route = {}
        route[str(method)] = action
        self.__routes[path] = route

    def __execute(self):
        while not self.__alive.is_set():
            try:
                client = self.__clients.get(timeout=1)
                if client:
                    self.__handle_client(client)
            except queue.Empty:
                continue

    def __read_request(self, client_socket):
        data = b""
        while True:
            chunk = client_socket.recv(4096)
            data += chunk
            if b"\r\n\r\n" in chunk:
                break

        return data.decode()

    def __parse_request(self, req: str) -> tuple:
        first_line = req.split("\r\n")[0]
        method, path, proto = first_line.split(" ")
        return method, path, proto

    def __handle_client(self, client_socket):
        req = self.__read_request(client_socket)
        method, path, _ = self.__parse_request(req)
        print(f"[*] Resource requested: {method} {path}")
        print(self.__routes)
        res = "".encode()
        try:
            html = ""
            action = self.__routes[path][method]
            if isinstance(action, str):
                with open(action, "r") as f:
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
