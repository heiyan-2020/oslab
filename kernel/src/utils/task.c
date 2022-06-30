#include <task.h>
#include <kmt.h>
extern task_list_t *tlist_;

void task_list_make(task_list_t *list) {
    list->head = pmm->alloc(sizeof(task_t));
    list->rear = pmm->alloc(sizeof(task_t));

    list->head->prev = list->rear;
    list->head->next = list->rear;
    list->rear->prev = list->head;
    list->rear->next = list->head; //circular link list.

}

void task_list_insert(task_list_t *list, task_t *node) {
    node->next = list->rear;
    node->prev = list->rear->prev;
    list->rear->prev->next = node;
    list->rear->prev = node;
}

void task_list_remove(task_list_t *list, task_t *node) {
    task_t *itr;
    for (itr = list->head->next; itr != list->rear; itr = itr->next) {
        if (itr == node) {
            break;
        }
    }

    panic_on(itr == list->rear, "Node not exists\n");
    node->prev->next = node->next;
    node->next->prev = node->prev;
    pmm->free(node);
}

task_t *task_list_pop(task_list_t *list) {
    task_t *ret = list->head->next;
    if (ret == list->rear) return NULL;

    task_list_remove(list, ret);
    return ret;
}

void task_init(task_t *task, Context *ctx, const char *name) {
    task->context = ctx;
    task->name = name;
    task->cpu = mycpu();
    task->state = RUNNABLE;
    task->alarm = false;
    task->child_ret = MAGIC_NUM;
    spin_init(&task->lk, name);
    task->pid = allocpid();
    if (task->pid > 5000) {
        while (1) {
            printf("sizeof(task_t) = %d\n", sizeof(task_t));
        }
    }
    for (int i = 0; i < NPAGES; i++) {
        task->vps[i] = NULL;
    }

    task_list_insert(tlist_, task);
}