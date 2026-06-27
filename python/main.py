import http_server


def print_hello():
    print("Hello World")
    return "<h1>Hello dad</h1>"


def main():
    server = http_server.HTTPServer(True)
    server.route("/", "GET", "../public/index.html")
    server.listen()


if __name__ == "__main__":
    main()
