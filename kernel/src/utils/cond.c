#include <cond.h>

void cond_init(cond_t *cond) {
    cond->head = pmm->alloc(sizeof(cond_node_t));
    cond->rear = pmm->alloc(sizeof(cond_node_t));

    cond->head->prev = NULL;
    cond->head->next = cond->rear;
    cond->rear->prev = cond->head;
    cond->rear->next = NULL;
}

void cond_push(cond_t *cond, task_t *task) {
    panic_on(ienabled(), "interrupt open\n");
    cond_node_t *node = pmm->alloc(sizeof(cond_node_t));
    node->task = task;
    node->prev = cond->rear->prev;
    node->next = cond->rear;
    cond->rear->prev->next = node;
    cond->rear->prev = node;
    panic_on(cond->head->next == cond->rear, "ahha\n");
}

task_t *cond_pop(cond_t *cond) {
    panic_on(ienabled(), "interrupt open\n");
    cond_node_t *node = cond->head->next;
    if (node == cond->rear) return NULL;

    node->next->prev = node->prev;
    node->prev->next = node->next;
    task_t *ret = node->task;
    pmm->free(node);
    return ret;
}