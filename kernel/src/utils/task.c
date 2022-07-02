#include <task.h>
#include <kmt.h>
#include <page.h>
extern task_list_t *tlist_;
extern spinlock_t schedule_lk;

void task_list_make(task_list_t *list) {
    list->head = pmm->alloc(sizeof(task_t));
    list->rear = pmm->alloc(sizeof(task_t));

    list->head->prev = list->rear;
    list->head->next = list->rear;
    list->rear->prev = list->head;
    list->rear->next = list->head; //circular link list.

}

void task_list_insert(task_list_t *list, task_t *node) {
    assert(holding(&schedule_lk));
    node->next = list->rear;
    node->prev = list->rear->prev;
    list->rear->prev->next = node;
    list->rear->prev = node;
}

void task_list_remove(task_list_t *list, task_t *node) {
    assert(holding(&schedule_lk));
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
    virt_list_make(&task->vps);

    task_list_insert(tlist_, task);
}