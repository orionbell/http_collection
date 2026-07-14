#include "include/http.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  HTTPServer *server = server_create(argc, argv);
  Route root = {
      .path = "/",
      .actionType = SRC_FILE,
      .method = GET,
      .action = "index.html",
  };
  define_route(server, root);
  if (server_listen(server)) {
    fprintf(stderr, "[!] Error: Listining failed\n");
    server_destroy(server);
    return EXIT_FAILURE;
  }
  server_destroy(server);
  return EXIT_SUCCESS;
}
