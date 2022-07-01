#include <page.h>
#include <os.h>

// static void page_list_insert(phypg_list_t *list, phypg_t *node);
// static void page_list_remove(phypg_list_t *list, phypg_t *node);

// void page_list_make(phypg_list_t *list) {
//     list->head = pmm->alloc(sizeof(phypg_t));
//     list->rear = pmm->alloc(sizeof(phypg_t));

//     list->head->prev = list->rear;
//     list->head->next = list->rear;
//     list->rear->prev = list->head;
//     list->rear->next = list->head; //circular link list.
// }

phypg_t *alloc_page(phypg_list_t *list, int pgsize) {
    phypg_t *page = pmm->alloc(sizeof(phypg_t));
    page->pa = pmm->alloc(pgsize);
    page->refcnt = 1;

    assert(page->pa != NULL);
    assert(page != NULL);

    // page_list_insert(list, page);

    return page;
}

// void release_page(phypg_list_t *list, phypg_t *page) {
//     assert(page != NULL);
//     assert(page->refcnt > 0);

//     page->refcnt--;

//     if (page->refcnt == 0) {
//         pmm->free(page->pa);
//         page_list_remove(list, page);
//     }
// }

// static void page_list_insert(phypg_list_t *list, phypg_t *node) {
//     node->next = list->rear;
//     node->prev = list->rear->prev;
//     list->rear->prev->next = node;
//     list->rear->prev = node;
// }

// static void page_list_remove(phypg_list_t *list, phypg_t *node) {
//     phypg_t *itr;
//     for (itr = list->head->next; itr != list->rear; itr = itr->next) {
//         if (itr == node) {
//             break;
//         }
//     }

//     panic_on(itr == list->rear, "Node not exists\n");

//     node->prev->next = node->next;
//     node->next->prev = node->prev;
//     pmm->free(node);
// }