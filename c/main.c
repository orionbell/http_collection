#include "include/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int supportAction(char **buf) {
  *buf = "<h1>Hi dad</h1>\n";
  return strlen(*buf);
}

int main(int argc, char *argv[]) {
  HTTPServer *server = server_create(argc, argv);
  Route root = {
      .path = "/",
      .actionType = SRC_FILE,
      .method = GET,
      .action = "../../public/index.html",
  };
  Route support = {
      .path = "/support",
      .actionType = FUNCTION,
      .method = GET,
      .action = supportAction,
  };
  if (define_route(server, root) == 0) {
    return EXIT_FAILURE;
  }
  if (define_route(server, support) == 0) {
    return EXIT_FAILURE;
  }
  if (server_listen(server)) {
    fprintf(stderr, "[!] Error: Listining failed\n");
    server_destroy(server);
    return EXIT_FAILURE;
  }
  server_destroy(server);
  return EXIT_SUCCESS;
}
