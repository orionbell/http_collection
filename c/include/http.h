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

typedef enum {
  SRC_FILE,
  FUNCTION,
} RouteAction;

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

typedef struct {
  HTTPMethod method;
  char *path;
  RouteAction actionType;
  void *action;
} Route;

typedef struct {
  Queue *clients;
  pthread_t *workers;
  struct sockaddr_in *addr;
  int sockfd;
  uint8_t workers_count;
  Route *routes;
  uint32_t routes_count;
  uint32_t routes_cap;
} HTTPServer;

HTTPServer *server_create(int argc, char *argv[]);
int server_listen(HTTPServer *server);
void server_destroy(HTTPServer *server);
int define_route(HTTPServer *server, Route route);
#endif // __HTTP_H_
