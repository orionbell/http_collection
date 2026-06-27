// Package http provides a basic http server
package http

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync"
	"syscall"
)

type HTTPMethod string

const (
	GET     HTTPMethod = "GET"
	POST    HTTPMethod = "POST"
	PUT     HTTPMethod = "PUT"
	DELETE  HTTPMethod = "DELETE"
	PATCH   HTTPMethod = "PATCH"
	HEAD    HTTPMethod = "HEAD"
	OPTIONS HTTPMethod = "OPTIONS"
	TRACE   HTTPMethod = "TRACE"
)

type Route map[string]any

type HTTPServer struct {
	addr   string
	port   int
	routes map[string]Route
	wg     sync.WaitGroup
}

func NewServer() *HTTPServer {
	return &HTTPServer{
		"",
		80,
		map[string]Route{},
		sync.WaitGroup{},
	}
}

func parseRequest(req string) (string, string) {
	firstLine, _, ok := strings.Cut(req, "\r\n")
	if !ok {
		return "", ""
	}
	requestParts := strings.Split(firstLine, " ")
	if len(requestParts) < 2 {
		return "", ""
	}
	return requestParts[0], requestParts[1]
}

func readRequest(conn net.Conn) ([]byte, error) {
	reader := bufio.NewReader(conn)
	var buffer bytes.Buffer
	for {
		chunk := make([]byte, 1024)
		bytesCount, err := reader.Read(chunk)
		if err != nil {
			if err == io.EOF {
				break
			} else {
				return nil, err
			}
		} else if bytesCount > 0 {
			buffer.Write(chunk[:bytesCount])
			if strings.Contains(string(chunk), "\r\n\r\n") {
				break
			}
		}
	}
	return buffer.Bytes(), nil
}

func (s *HTTPServer) handleClient(conn net.Conn) {
	defer conn.Close()
	data, err := readRequest(conn)
	if err != nil {
		log.Printf("[!] Error: %s\n", err)
		return
	}
	method, url := parseRequest(string(data))
	if method == "" || url == "" {
		log.Println("[!] Error Failed to parse request.")
		return
	}
	log.Printf("[*] Request: %s %s\n", method, url)
	action, ok := s.routes[url][method]
	if !ok {
		log.Printf("[!] Error: Invalid request %s %s\n", method, url)
		_, err := conn.Write([]byte("HTTP/1.1 404 Not Found\r\n\r\n"))
		if err != nil {
			log.Printf("[!] Error: Failed to send response to client, %s\n", err)
		}
		return
	}
	switch action := action.(type) {
	case func() string:
		html := action()
		if html != "" {
			_, err = fmt.Fprintf(conn, "HTTP/1.1 200 OK\r\nContent-length: %d\r\nContent-Type: text/html\r\n\r\n%s\r\n", len(html), html)
		} else {
			_, err = conn.Write([]byte("HTTP/1.1 200 OK\r\n\r\n"))
		}
		if err != nil {
			log.Printf("Error: Failed to send response to client, %s\n", err)
			return
		}
	case string:
		html, err := os.ReadFile(action)
		if err != nil {
			log.Fatalf("[!] Error: Failed to read file - %s", err)
		}
		_, err = fmt.Fprintf(conn, "HTTP/1.1 200 OK\r\nContent-length: %d\r\nContent-Type: text/html\r\n\r\n%s\r\n", len(html), html)
		if err != nil {
			log.Printf("Error: Failed to send response to client, %s\n", err)
			return
		}
	case any:
	}
}

func (s *HTTPServer) SetAddress(addr string) *HTTPServer {
	s.addr = addr
	return s
}

func (s *HTTPServer) SetPort(port int) *HTTPServer {
	s.port = port
	return s
}

func (s *HTTPServer) UseCliArgs() *HTTPServer {
	if len(os.Args) >= 3 {
		s.addr = os.Args[1]
		tmp, err := strconv.Atoi(os.Args[2])
		if err == nil {
			s.port = tmp
		} else {
			log.Printf("[!] Error: Invalid port %s, update failed\n", os.Args[2])
		}
	} else if len(os.Args) >= 2 {
		s.addr = os.Args[1]
	}
	return s
}

func (s *HTTPServer) NewRoute(path string, method HTTPMethod, action any) *HTTPServer {
	route := Route{}
	route[string(method)] = action
	s.routes[path] = route
	return s
}

func acceptClients(server net.Listener, clientChan chan net.Conn) {
	for {
		clientConn, err := server.Accept()
		if err != nil {
			log.Printf("[!] Error: %s\n", err)
			continue
		}
		clientChan <- clientConn
	}
}

func (s *HTTPServer) Listen() {
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	clientChan := make(chan net.Conn)
	server, err := net.Listen("tcp", fmt.Sprintf("%s:%d", s.addr, s.port))
	if err != nil {
		log.Fatalf("[!] Error: %s\n", err)
	}
	log.Printf("[*] Server is listening on %s\n", server.Addr())
	defer server.Close()
	go acceptClients(server, clientChan)
	for {
		select {
		case <-ctx.Done():
			cancel()
			s.wg.Wait()
			log.Println("[^] Recived CTRL+C. Exiting...")
			os.Exit(0)
		case clientConn := <-clientChan:
			log.Printf("[*] New connection from %s\n", clientConn.RemoteAddr())
			s.wg.Go(func() {
				s.handleClient(clientConn)
			})
		}
	}
}
