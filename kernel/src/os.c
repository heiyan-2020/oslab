#include <os.h>
#include <stdint.h>
#include <handler.h>
#include <config.h>
#include <test.h>
#include <uproc.h>
/*============================================
                  declaration
=============================================*/
static void os_init();
static void os_run();
Context * trap(Event ev, Context *context);
void on_irq(int seq, int event, handler_t handler);
/*============================================
                  interfaces
=============================================*/
MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = trap,
  .on_irq = on_irq
};
/*============================================
                global variables
=============================================*/
handler_list_t *hlist_;
/*============================================
              implementations
=============================================*/
static void os_init() {
    /*pmm*/
    pmm->init();

    /*kmt*/
    hlist_ = pmm->alloc(sizeof(handler_list_t));
    handler_list_make(hlist_);
    kmt->init();

    /*uproc*/
    // uproc->init();
    LOG("");

#ifdef LOCAL_DEV
    dev->init();
#endif

#ifdef TEST_LAB2
    create_test();
#endif

#ifdef TEST_LAB3
    task_t *task = pmm->alloc(sizeof (task_t));
    ucreate(task, "init");
#endif

}
extern unsigned int _init_len;
static void os_run() {
    printf("Hello, World\n");
    iset(true);
    while (1);
}

Context *trap(Event ev, Context *context) {
    Context *next = NULL;
    for (handler_node_t *itr = hlist_->head->next; itr != hlist_->rear; itr = itr->next) {
        if (itr->event == EVENT_NULL || itr->event == ev.event) {
            Context *temp = itr->handler(ev, context);
            panic_on(temp && next, "Return multiple context\n");
            if (temp) next = temp;
        }
    }
    panic_on(!next, "Return NULL context\n");
    panic_on(ienabled(), "trap-interruptable\n");

    return next;
}

void on_irq(int seq, int event, handler_t handler) {
    handler_node_t *node = pmm->alloc(sizeof(handler_node_t));
    node->seq = seq;
    node->event = event;
    node->handler = handler;
    handler_list_insert(hlist_, node);
}