#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct Item {
  void *value;
  struct Item *next;
} Item;

typedef struct {
  Item *head;
  Item *tail;
  unsigned int length;
} Queue;

Queue *queue_create();
int put(Queue *queue, void *value);
void *get(Queue *queue);
void queue_destroy(Queue *queue);

#endif // __QUEUE_H_
