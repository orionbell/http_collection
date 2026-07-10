#include "../include/http.h"
#include "../include/queue.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 8080
#define DEFAULT_TH_COUNT 32
#define CHUNK_SIZE 1024

pthread_mutex_t mutex;
bool alive = true;

void handle_ctrlc(int n) {
  pthread_mutex_lock(&mutex);
  alive = false;
  pthread_mutex_unlock(&mutex);
  printf(" SIGKILL detected, exiting ...\n");
}

char *read_request(int clientfd);
char **parse_request();

void handle_client(int clientfd);

void *execute(void *arg) {
  HTTPServer *server = (HTTPServer *)arg;
  if (server == NULL)
    return NULL;
  while (1) {
    pthread_mutex_lock(&mutex);
    if (server->clients->length > 0) {
      void *client = get(((HTTPServer *)server)->clients);
    }
    if (alive) {
      pthread_mutex_unlock(&mutex);
      break;
    }
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

HTTPServer *server_create(int argc, char *argv[]) {
  HTTPServer *server = (HTTPServer *)malloc(sizeof(HTTPServer));
  pthread_mutex_init(&mutex, NULL);
  signal(SIGINT, handle_ctrlc);
  if (server == NULL)
    return NULL;
  server->addr = malloc(sizeof(struct sockaddr_in));
  if (server->addr == NULL)
    return NULL;
  memset(server->addr, 0, sizeof(struct sockaddr_in));
  server->addr->sin_family = AF_INET;
  if (inet_aton(DEFAULT_ADDR, &server->addr->sin_addr) != 1) {
    perror("[!] Failed to parse address, error");
    return NULL;
  }
  server->addr->sin_port = htons(DEFAULT_PORT);
  if (argc >= 2) {
    if (argc >= 3 && isdigit(argv[2]) && atoi(argv[2]) > 0) {
      server->addr->sin_port = htons(atoi(argv[2]));
    } else {
      if (argc > 3) {
        fprintf(stderr,
                "Warning: Invalid port value: %s, using default port (%d).",
                argv[2], DEFAULT_PORT);
      }
    }
    if (inet_aton(argv[1], &server->addr->sin_addr) != 1) {
      fprintf(stderr,
              "Warning: Invalid address value: %s, using default address (%s).",
              argv[1], DEFAULT_ADDR);
    }
  }
  server->sockfd = 0;
  server->workers_count = 0;
  if (argc >= 4)
    server->workers_count = atoi(argv[3]);
  if (server->workers_count == 0)
    server->workers_count = 32;
  server->workers = malloc(sizeof(pthread_t) * server->workers_count);
  server->clients = queue_create();
  return server;
}
int server_listen(HTTPServer *server) {
  if (server == NULL)
    return 1;

  server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->sockfd == -1) {
    perror("[!] Failed to create socket, error");
    return 1;
  }
  int flags = fcntl(server->sockfd, F_GETFL, 0);
  fcntl(server->sockfd, F_SETFL, flags | O_NONBLOCK);
  for (int i = 0; i < server->workers_count; i++) {
    pthread_create(&(server->workers[i]), NULL, execute, server);
  }
  if (bind(server->sockfd, (struct sockaddr *)server->addr,
           sizeof(*server->addr)) == -1) {
    perror("[!] Failed to bind server, error");
    close(server->sockfd);
    return 1;
  }
  if (listen(server->sockfd, 0) == -1) {
    perror("[!] Failed to listen on server, error");
    close(server->sockfd);
    return 1;
  }

  printf("[+] Server is listining on %s:%d\n",
         inet_ntoa(server->addr->sin_addr), ntohs(server->addr->sin_port));
  struct sockaddr_in client_addr;
  socklen_t client_addr_len;
  while (1) {
    int clientfd;
    if ((clientfd = accept(server->sockfd, (struct sockaddr *)&client_addr,
                           &client_addr_len)) != -1) {
      printf("[+] New Request from %s:%d\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      pthread_mutex_lock(&mutex);
      if (alive) {
        put(server->clients, (void *)&clientfd);
      }
      pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_lock(&mutex);
    if (!alive) {
      pthread_mutex_unlock(&mutex);
      break;
    }
    pthread_mutex_unlock(&mutex);
  }
  printf("[+] Joining Threads...\n");
  for (int i = 0; i < server->workers_count; i++) {
    pthread_join(server->workers[i], NULL);
  }
  close(server->sockfd);
  printf("[+] Done\n");
  return 0;
}
void server_destroy(HTTPServer *server) {
  if (server == NULL)
    return;
  queue_destroy(server->clients);
  pthread_mutex_destroy(&mutex);
  free(server);
}
