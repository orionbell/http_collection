#include "include/queue.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  Queue *queue = queue_create();
  put(queue, "Hello");
  put(queue, "Test");
  put(queue, "World");
  printf("%s\n", (char *)get(queue));
  queue_destroy(queue);
  return 0;
}
