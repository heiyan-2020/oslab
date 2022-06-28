#include <os.h>
/*============================================
                declarations
=============================================*/
typedef struct _node {
  int seq;
  int event;
  handler_t handler;
  struct _node *next;
  struct _node *prev;
} handler_node_t;

typedef struct {
    handler_node_t *head;
    handler_node_t *rear;
} handler_list_t;

void handler_list_make(handler_list_t *list);
void handler_list_insert(handler_list_t *list, handler_node_t *node);
// void handler_list_remove(handler_list_t *list, handler_node_t *node);