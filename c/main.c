#include "include/http.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  HTTPServer *server = server_create(argc, argv);
  if (server_listen(server)) {
    fprintf(stderr, "[!] Error: Listining failed\n");
    server_destroy(server);
    return EXIT_FAILURE;
  }
  server_destroy(server);
  return EXIT_SUCCESS;
}
