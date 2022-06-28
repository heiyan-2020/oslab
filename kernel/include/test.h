#include <config.h>
#include <os.h>
#ifdef TEST_LAB2
#define P kmt->sem_wait
#define V kmt->sem_signal
void spin_test();
void sem_test();
void create_test();


void exit();
task_t *mytask();
#endif