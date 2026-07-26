#include "../include/http.h"
#include "../include/queue.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <fcntl.h>
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
#define ROUTES_ARR_SIZE 8
#define DEFAULT_RES_HEADERS_LEN                                                \
  strlen("HTTP/1.1 200 OK\r\nContent-length: %d\r\nContent-Type: "             \
         "text/html\r\nConnection: close\r\n\r\n")
bool is_listing = false;

pthread_mutex_t mutex;
bool alive = true;

unsigned int read_file(const char *filename, char **buffer) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("[!] Failed to open file, error");
    return -1;
  }
  *buffer = malloc(CHUNK_SIZE + 1); // + 1 for the null character
  char *content = *buffer;
  if (content == NULL) {
    perror("[!] Memory error");
    return -1;
  }
  unsigned int i;
  for (i = 0; (content[i] = fgetc(file)) != EOF; i++) {
    if (i % CHUNK_SIZE == 0) {
      content = realloc(content, CHUNK_SIZE);
      if (content == NULL) {
        perror("[!] Memory error");
        return -1;
      }
    }
  }
  fclose(file);
  return i;
}

void handle_ctrlc(int n) {
  pthread_mutex_lock(&mutex);
  alive = false;
  pthread_mutex_unlock(&mutex);
  printf(" SIGKILL detected, exiting ...\n");
}
unsigned int read_request(int clientfd, char **req_buf) {
  char *buffer = malloc(CHUNK_SIZE);
  char chunk[CHUNK_SIZE];
  int buffer_len = 0, chunk_len = 0;
  while ((chunk_len = read(clientfd, chunk, CHUNK_SIZE)) > 0) {
    if (chunk_len == CHUNK_SIZE) {
      buffer = realloc(buffer, sizeof(buffer) + CHUNK_SIZE);
    }
    memcpy(buffer + buffer_len, chunk, chunk_len);
    buffer_len += chunk_len;
    if (strstr(buffer, "\r\n\r\n") != NULL)
      break;
  }
  if (chunk_len < 0) {
    perror("[!] Failed to read from socket, error");
    req_buf = NULL;
    return -1;
  }
  *req_buf = buffer;
  return buffer_len;
}
void parse_request(char **req_buf, HTTPMethod *method, char **path) {
  char *method_str = strtok(*req_buf, " ");
  *path = strtok(NULL, " ");
  if (strcmp(method_str, "GET") == 0)
    *method = GET;
  else if (strcmp(method_str, "POST") == 0)
    *method = POST;
  else if (strcmp(method_str, "PUT") == 0)
    *method = PUT;
  else if (strcmp(method_str, "DELETE") == 0)
    *method = DELETE;
  else if (strcmp(method_str, "HEAD") == 0)
    *method = HEAD;
  else if (strcmp(method_str, "HEAD") == 0)
    *method = OPTIONS;
  else if (strcmp(method_str, "TRACE") == 0)
    *method = TRACE;
  else if (strcmp(method_str, "PATCH") == 0)
    *method = PATCH;
};
void handle_client(HTTPServer *server, int clientfd) {
  char *req_buf;
  HTTPMethod method;
  char *path;
  char *method_str;
  read_request(clientfd, &req_buf);
  parse_request(&req_buf, &method, &path);
  switch (method) {
  case GET:
    method_str = "GET";
    break;
  case POST:
    method_str = "POST";
    break;
  case PUT:
    method_str = "PUT";
    break;
  case DELETE:
    method_str = "DELETE";
    break;
  case OPTIONS:
    method_str = "OPTIONS";
    break;
  case HEAD:
    method_str = "HEAD";
    break;
  case PATCH:
    method_str = "PATCH";
    break;
  case TRACE:
    method_str = "TRACE";
    break;
  }
  printf("[+] Request: %s %s\n", method_str, path);
  char *html;
  unsigned int length = 0;
  for (int i = 0; i < server->routes_count; i++) {
    if (strcmp(server->routes[i].path, path) == 0 &&
        server->routes[i].method == method) {
      if (server->routes[i].actionType == SRC_FILE ||
          server->routes[i].actionType == FUNCTION) {
        if (server->routes[i].actionType == SRC_FILE)
          length = read_file(server->routes[i].action, &html);
        else
          length = ((unsigned int (*)(char **))server->routes[i].action)(&html);
        char *res = malloc(DEFAULT_RES_HEADERS_LEN + length + 4);
        snprintf(res, DEFAULT_RES_HEADERS_LEN + length + 4,
                 "HTTP/1.1 200 OK\r\nContent-length: %d\r\nContent-Type: "
                 "text/html\r\nConnection: close\r\n\r\n%.*s\r\n",
                 length, length, html);
        if (send(clientfd, res, strlen(res), 0) < 0)
          perror("[!] Failed to send response, error");
        close(clientfd);
        free(req_buf);
        free(res);
        return;
      }
    }
  }
  char *not_found_res = "HTTP/1.1 404 Not Found\r\n";
  if (send(clientfd, not_found_res, strlen(not_found_res), 0) < 0) {
    perror("[!] Failed to send response, error");
  } else {
    printf("[+] Route %s %s not found\n", method_str, path);
  }
  close(clientfd);
  free(req_buf);
  return;
}
void *execute(void *arg) {
  HTTPServer *server = (HTTPServer *)arg;
  if (server == NULL)
    return NULL;
  while (1) {
    pthread_mutex_lock(&mutex);
    if (server->clients->length > 0) {
      int *clientfd = (int *)get(((HTTPServer *)server)->clients);
      handle_client(server, *clientfd);
    }
    if (!alive) {
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
  if (server == NULL) {
    perror("Memory error");
    return NULL;
  }
  server->addr = malloc(sizeof(struct sockaddr_in));
  if (server->addr == NULL) {
    perror("Memory error");
    return NULL;
  }
  memset(server->addr, 0, sizeof(struct sockaddr_in));
  server->addr->sin_family = AF_INET;
  if (inet_aton(DEFAULT_ADDR, &server->addr->sin_addr) != 1) {
    perror("[!] Failed to parse address, error");
    return NULL;
  }
  server->addr->sin_port = htons(DEFAULT_PORT);
  if (argc >= 2) {
    if (argc >= 3 && atoi(argv[2]) > 0) {
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
  server->routes = malloc(sizeof(Route) * ROUTES_ARR_SIZE);
  if (server->routes == NULL) {
    perror("Memory error");
    return NULL;
  }
  server->routes_count = 0;
  server->routes_cap = ROUTES_ARR_SIZE;
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
  if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1},
                 sizeof(int)) < 0 ||
      setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1},
                 sizeof(int)) < 0) {
    perror("[!] Failed to configure socket, error");
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
  is_listing = true;
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
  free(server->routes);
  free(server);
}
int define_route(HTTPServer *server, Route route) {
  if (is_listing)
    return 0;
  if (server == NULL)
    return 0;
  if (server->routes_count >= server->routes_cap) {
    if ((server->routes = realloc(
             server->routes, server->routes_cap + ROUTES_ARR_SIZE)) == NULL) {
      perror("Memory error");
      return 0;
    }
    server->routes_cap += ROUTES_ARR_SIZE;
  }
  if (route.actionType == SRC_FILE) {
    FILE *file = fopen(route.action, "r");
    if (file == NULL) {
      perror("Failed to open file, error");
      return 0;
    }
    fclose(file);
  }
  server->routes[server->routes_count] = route;
  server->routes_count++;
  return 1;
}
