package main

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
	"strings"
	"sync"
	"syscall"
)

func getNetInfo() (string, string) {
	port := "80"
	var ip string
	if len(os.Args) >= 3 {
		ip = os.Args[1]
		port = os.Args[2]
	} else if len(os.Args) >= 2 {
		ip = os.Args[1]
	}
	return ip, port
}

func readRequest(conn net.Conn) ([]byte, error) {
	log.Println("Reading request")
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

func handleClient(conn net.Conn) {
	defer conn.Close()
	data, err := readRequest(conn)
	if err != nil {
		log.Fatalf("[!] Error: %s\n", err)
	}
	fmt.Printf(`%s`, data)
}

func acceptClients(server net.Listener, clientChan chan net.Conn) {
	for {
		clientConn, err := server.Accept()
		if err != nil {
			log.Fatalf("[!] Error: %s\n", err)
		}
		clientChan <- clientConn
	}
}

func main() {
	var wg sync.WaitGroup
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	ip, port := getNetInfo()
	var server net.Listener
	clientChan := make(chan net.Conn)
	var err error
	if ip != "" {
		server, err = net.Listen("tcp", fmt.Sprintf("%s:%s", ip, port))
	} else {
		server, err = net.Listen("tcp", fmt.Sprintf(":%s", port))
	}
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
			wg.Wait()
			log.Println("Recived CTRL+C. Exiting...")
			os.Exit(0)
		case clientConn := <-clientChan:
			log.Printf("[*] New connection from %s\n", clientConn.RemoteAddr())
			wg.Go(func() {
				handleClient(clientConn)
			})
		}
	}
}
