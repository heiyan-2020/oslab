#include <config.h>
#include <test.h>
#include <os.h>
#ifdef TEST_LAB2


spinlock_t outer, inner, sum_lk, print_lk;

int sum=0;
void shared_sum() {
    for (int i = 0; i < 100000; i++) {
        kmt->spin_lock(&sum_lk);
        sum++;
        kmt->spin_unlock(&sum_lk);
    }

    kmt->spin_lock(&sum_lk);
    printf("sum=%d\n", sum);
    kmt->spin_unlock(&sum_lk);

    while (true) {
        yield();
    }
}

void just_yield() {
    while (true) {
        yield();
    }
}
int i = 0;
void print_star() {
    while (true) {
        kmt->spin_lock(&print_lk);
        printf("(%d)",i++);
        kmt->spin_unlock(&print_lk);

        for (volatile int i = 0; i < 100000; i++);
    }
}

sem_t empty, fill;
void producer(void *arg) { 
  while (1) {
    P(&empty); 
    printf("(");
    V(&fill);  
  } 
}

void consumer(void *arg) { 
    while (1) { 
      P(&fill);
      printf(")");  
      V(&empty); 
    } 
}

void suicide(void *arg) {
    printf("%d\n", mytask()->pid);
    for (int i = 0; i < 1; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "suicide", suicide, NULL);
    }
    exit();
}

void spin_test() {
    kmt->spin_init(&sum_lk, "sum_lk");
    kmt->spin_init(&print_lk, "print_lk");

    for (int i = 0; i < 2; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "star", print_star, NULL);
    }

    for (int i = 0; i < 10; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "yield", just_yield, NULL);
    }

    for (int i = 0; i < 10; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "print", print_star, NULL);
    }
}

void sem_test() {
    kmt->sem_init(&empty, "empty", 1);
    kmt->sem_init(&fill, "fill", 0);

    for (int i = 0 ; i < 10; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "producer", producer, NULL);
    }

    for (int i = 0; i < 10; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "consumer", consumer, NULL);
    }

    for (int i = 0; i < 20; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "yield", just_yield, NULL);
    }

    for (int i = 0; i < 10; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "sum", print_star, NULL);
    }
}

void create_test() {
    kmt->sem_init(&empty, "empty", 1);
    kmt->sem_init(&fill, "fill", 0);

    for (int i = 0; i < 1; i++) {
        task_t *task = pmm->alloc(sizeof(task_t));
        kmt->create(task, "suicide", suicide, NULL);
    }
}



#endif