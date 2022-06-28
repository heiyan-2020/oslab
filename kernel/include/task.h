#include <os.h>
/*============================================
                declarations
=============================================*/

void task_list_make(task_list_t *list);
void task_list_insert(task_list_t *list, task_t *node); //design decision: push_back
void task_list_remove(task_list_t *list, task_t *node);
task_t *task_list_pop(task_list_t *list);
void task_init(task_t *task, Context *ctx, const char *name);