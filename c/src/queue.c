#include "../include/queue.h"
#include <stdlib.h>

Queue *queue_create() {
  Queue *queue = (Queue *)malloc(sizeof(Queue));
  if (queue == NULL)
    return NULL;
  queue->head = NULL;
  queue->tail = NULL;
  queue->length = 0;
  return queue;
}

int put(Queue *queue, void *value) {
  Item *item = (Item *)malloc(sizeof(Item));
  if (item == NULL)
    return 1;
  item->value = value;
  item->next = NULL;
  if (queue->length > 0)
    queue->tail->next = item;
  else
    queue->head = item;
  queue->tail = item;
  queue->length++;
  return 0;
}
void *get(Queue *queue) {
  if (queue->length <= 0)
    return NULL;
  Item *head = queue->head;
  queue->head = head->next;
  void *val = head->value;
  free(head);
  queue->length--;
  return val;
}
void queue_destroy(Queue *queue) {
  if (queue == NULL)
    return;
  Item *head = queue->head;
  for (int i = 0; i < queue->length; i++) {
    queue->head = head->next;
    free(head);
    head = queue->head;
  }
  free(queue);
}
