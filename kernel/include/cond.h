#include <os.h>

void cond_init(cond_t *cond);
void cond_push(cond_t *cond, task_t *task);
task_t *cond_pop(cond_t *cond);