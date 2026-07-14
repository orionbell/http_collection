#ifndef _HTTP_H_
#define _HTTP_H_
#include "pthread.h"
#include "queue.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
  Queue *clients;
  pthread_t *workers;
  struct sockaddr_in *addr;
  int sockfd;
  uint8_t workers_count;
} HTTPServer;

typedef enum {
  GET,
  POST,
  DELETE,
  PUT,
  OPTIONS,
  PATCH,
  HEAD,
  TRACE,
} HTTPMethod;

HTTPServer *server_create(int argc, char *argv[]);
int use_cli_args(HTTPServer *server);
int server_listen(HTTPServer *server);
void server_destroy(HTTPServer *server);

#endif // __HTTP_H_
