#ifndef _HTTP_H_
#define _HTTP_H_

typedef struct {
  unsigned short port;
  char addr[];
} HTTPServer;

int server_create(HTTPServer *server);
int use_cli_args(HTTPServer *server);
int listen(HTTPServer *server);
int server_destroy(HTTPServer *server);

#endif // __HTTP_H_
