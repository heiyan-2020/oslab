#include <handler.h>

void handler_list_make(handler_list_t *list) {
    list->head = pmm->alloc(sizeof(handler_node_t));
    list->rear = pmm->alloc(sizeof(handler_node_t));

    list->head->prev = NULL;
    list->head->next = list->rear;
    list->rear->prev = list->head;
    list->rear->next = NULL;
}

//insert node according to node->seq
void handler_list_insert(handler_list_t *list, handler_node_t *node) {
    handler_node_t *itr;
    for (itr = list->head->next; itr != list->rear; itr = itr->next) {
        if (itr->seq > node->seq) {
            break;
        }
    }
    //invirants: itr is the first node whose seq > node->seq or rear.
    node->next = itr;
    node->prev = itr->prev;

    itr->prev->next = node;
    itr->prev = node;
}