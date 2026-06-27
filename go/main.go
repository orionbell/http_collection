package main

import "http-server/http"

func main() {
	server := http.NewServer().UseCliArgs()
	server.NewRoute("/", http.GET, "../public/index.html")
	server.NewRoute("/test", http.POST, func() string { return "<h1>Hi mom</h1>" })
	server.Listen()
}
