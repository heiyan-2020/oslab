#include <kmt.h>
static void idles_init();
static void cpus_init();
/*============================================
                  interfaces
=============================================*/
MODULE_DEF(kmt) = {
    .init = kmt_init,
    .create = kcreate,
    .teardown = teardown,
    .spin_init = spin_init,
    .spin_lock = spin_lock,
    .spin_unlock = spin_unlock,
    .sem_init = sem_init,
    .sem_wait = sem_wait,
    .sem_signal = sem_signal
};
/*============================================
              global variables
=============================================*/
task_t *idles_[NCPU];
task_list_t *tlist_;
cpu_t cpus_[NCPU];
spinlock_t schedule_lk;
int pid_queue[PID_BOUND + 1], pid_front = 0, pid_rear = 0; //用于维护pid的获取/释放

/*  state handler for scheduler */
static state_handler_t state_table[] = {
    [RUNNING] = handler_DEFAULT,
    [RUNNABLE] = handler_ERROR,
    [SLEEPY] = handler_SLEEPY,
    [SLEEPING] = handler_ERROR,
    [BLOCKED] = handler_ERROR,
    [DEAD] = handler_DEAD,
};

#define NONE -1
/*============================================
              implementations
=============================================*/
void kmt_init() {
    os->on_irq(INT32_MAX, EVENT_NULL, schedule);

    initpid();
    idles_init();
    cpus_init();

    tlist_ = pmm->alloc(sizeof(task_list_t));
    task_list_make(tlist_);
    tlist_->head->state = BLOCKED;
    tlist_->rear->state = BLOCKED;

    spin_init(&schedule_lk, "schuedle_lk");
#ifdef LOCAL_LOG
    LOG_INIT();
#endif
}

int kcreate(task_t *task, const char *name, void (*entry)(void *arg), void *arg) {
    panic_on(task == NULL, "Task should be allocated before\n");

    spin_lock(&schedule_lk);
    Context *ctx = kcontext((Area) {&(task->stack), task + 1}, entry, arg);
    task_init(task, ctx, name);
    spin_unlock(&schedule_lk);
    return 0;
}

void teardown(task_t *task) {
    panic_on(task->state != DEAD, "Reap some active tasks\n");
    panic_on(schedule_lk.locked == 0, "schedule_lk should be locked\n");
    task_list_remove(tlist_, task);
}

void spin_init(spinlock_t *lk, const char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = NULL;
}

void spin_lock(spinlock_t *lk) {
    push_off();
    // LOG("%s\n", lk->name);
    COND_LOG(holding(lk), "double acquire of %s[%d]\n",lk->name, lk->cpu->cpuno);
    panic_on(holding(lk), "double acquire\n");
    while (atomic_xchg(&lk->locked, 1));
    lk->cpu = mycpu();
}

void spin_unlock(spinlock_t *lk) {
    // LOG("%s\n", lk->name);
    COND_LOG(!holding(lk), "release not holding lock %s\n",lk->name);
    panic_on(!holding(lk), "release not holding lock\n");
    lk->cpu = NULL;
    atomic_xchg(&lk->locked, 0);
    pop_off();
}

void sem_init(sem_t *sem, const char *name, int value) {
    spin_init(&sem->lk, name);
    cond_init(&sem->cond);
    sem->value = value;
    sem->name = name;
}

#ifdef SEM_MODEL
void sem_wait(sem_t *sem) {
    int succ = false;
    while (succ == false) {
        spin_lock(&sem->lk);
        if (sem->value > 0) {
            sem->value--;
            succ = true;
        }
        spin_unlock(&sem->lk);
        if (!succ) yield();
    }
}

void sem_signal(sem_t *sem) {
    spin_lock(&sem->lk);
    sem->value++;
    spin_unlock(&sem->lk);
}
#endif

#ifdef SEM_SLEEP
void sem_wait(sem_t *sem) {
    spin_lock(&sem->lk);
    while (sem->value <= 0) {
        sleep(&sem->cond, &sem->lk);
    }
    sem->value--;
    spin_unlock(&sem->lk);
}

void sem_signal(sem_t *sem) {
    spin_lock(&sem->lk);
    sem->value++;
    wakeup(&sem->cond);
    spin_unlock(&sem->lk);
}
#endif

Context *schedule(Event ev, Context *context) {
    panic_on(ienabled(), "\n");

    spin_lock(&schedule_lk);
    task_t *cur = mytask();

    task_t *round_begin = cur == idles_[mycpu()->cpuno] ? tlist_->head : cur->next;
    task_t *itr = round_begin;

    state_table[cur->state](cur, context); //查表状态迁移

    do {
        if (itr->state == RUNNABLE && check_race(itr)) {

            itr->state = RUNNING;
            itr->cpu = mycpu();

            mycpu()->cur = itr;
            mycpu()->prev = cur;
            LOG("prev:[%s], current:[%s]\n", cur->name, itr->name);
            panic_on(ienabled(), "\n");
            spin_unlock(&schedule_lk);
            return itr->context;
        }
        itr = itr->next;
    } while (itr != round_begin);

    itr = idles_[mycpu()->cpuno];
    itr->state = RUNNING;
    itr->cpu = mycpu();
    mycpu()->cur = itr;
    mycpu()->prev = cur;
    LOG("prev:[%s], current:[%s]\n", cur->name, itr->name);
    panic_on(ienabled(), "\n");
    spin_unlock(&schedule_lk);
    return itr->context;
}

static void idles_init() {
    for (int i = 0; i < cpu_count(); i++) {
        idles_[i] = pmm->alloc(sizeof(task_t));
        task_t *task = idles_[i];
        task->cpu = &cpus_[i];
        char *name = pmm->alloc(16);
        sprintf(name, "idle%d", i);
        task->name = name;
        task->state = RUNNING;
        spin_init(&task->lk, task->name);
        task->pid = allocpid();
    }
}

static void cpus_init() {
    for (int i = 0; i < cpu_count(); i++) {
        cpus_[i].cpuno = i;
        cpus_[i].intena = false;
        cpus_[i].noff = 0;
        cpus_[i].cur = idles_[i];
        cpus_[i].prev = NULL;
    }
}

cpu_t *mycpu() {
    int id = cpu_current();
    cpu_t *c = &cpus_[id];
    return c;
}

task_t *mytask() {
    push_off();
    cpu_t *c = mycpu();
    task_t *t = c->cur;
    pop_off();
    return t;
}

void push_off() {
    int old = ienabled();

    iset(false);
    if (mycpu()->noff == 0) {
        mycpu()->intena = old;
    }
    mycpu()->noff++;
}

void pop_off() {
    cpu_t *c = mycpu();
    COND_LOG(ienabled(), "pop_off - interruptible\n");
    COND_LOG(c->noff < 1, "pop_off\n");
    c->noff--;
    if (c->noff == 0 && c->intena) {
        iset(true);
    }
}

int holding(spinlock_t *lk) {
    return lk->locked && lk->cpu == mycpu();
}

int check_race(task_t *task) {
    panic_on(ienabled(), "\n");
    for (int i = 0; i < cpu_count(); i++) {
        if (mycpu() != &cpus_[i] && task == cpus_[i].prev) {
            return false;
        }
    }
    panic_on(ienabled(), "\n");
    return true;
}

void sleep(cond_t *cond, spinlock_t *lk) {
    task_t *cur = mytask();

    spin_lock(&cur->lk);

    cur->state = SLEEPY;
    cond_push(cond, cur);
    spin_unlock(lk);
    spin_unlock(&cur->lk);
    yield();
    spin_lock(lk);
}

void wakeup(cond_t *cond) {
    task_t *sleeper = cond_pop(cond);
    if (sleeper == NULL) {
        return;
    }

    spin_lock(&sleeper->lk);
    panic_on(sleeper->state != SLEEPY && sleeper->state != SLEEPING, "\n");
    if (sleeper->state == SLEEPY) {
        sleeper->alarm = true; //此时sleep线程已经被切换
    } else {
        sleeper->state = RUNNABLE; //此时sleep线程还未被切换
    }
    spin_unlock(&sleeper->lk);
}

void handler_DEFAULT(task_t *task, Context *ctx) {
    panic_on(ienabled(), "Interrupt enabled in scheculer\n");
    spin_lock(&task->lk);
    task->context = ctx;
    task->state = RUNNABLE;
    task->alarm = false;
    spin_unlock(&task->lk);
}

void handler_SLEEPY(task_t *task, Context *ctx) {
    panic_on(task->state != SLEEPY, "Unsleepy thread enters sleepy handler\n");
    panic_on(ienabled(), "Interrupt enabled in scheculer\n");
    spin_lock(&task->lk);
    task->context = ctx;
    if (task->alarm == false) {
        task->state = SLEEPING;
    }
    spin_unlock(&task->lk);
}

void handler_DEAD(task_t *task, Context *ctx) {
    panic_on(task->state != DEAD, "Unsleepy thread enters sleepy handler\n");
    panic_on(ienabled(), "Interrupt enabled in scheculer\n");
    
    teardown(task);
}

void handler_ERROR(task_t *task, Context *ctx) {
    panic_on(ienabled(), "Interrupt enabled in scheculer\n");
    panic("Kernel Thread has wrong state.\n");
}

void initpid() {
    panic_on(pid_front != pid_rear, "PID queue isn't empty at first\n");
    for (int i = 0; i < PID_BOUND; i++) {
        freepid(i + 1);
    }
}

int allocpid() {
    panic_on(pid_rear == pid_front, "Already empty\n");
    int ret = pid_queue[pid_front];
    pid_front = (pid_front + 1) % (PID_BOUND + 1);
    return ret;
}

void freepid(int pid) {
    panic_on((pid_rear + 1) % (PID_BOUND + 1) == pid_front, "Already Full\n");
    pid_queue[pid_rear] = pid;
    pid_rear = (pid_rear + 1) % (PID_BOUND + 1);
}

void exit() {
    spin_lock(&mytask()->lk);
    mytask()->state = DEAD;
    spin_unlock(&mytask()->lk);

    yield();
}