#include <page.h>
#include <os.h>

// static void phy_list_insert(phypg_list_t *list, phypg_t *node);
// static void phy_list_remove(phypg_list_t *list, phypg_t *node);

// void phy_list_make(phypg_list_t *list) {
//     list->head = pmm->alloc(sizeof(phypg_t));
//     list->rear = pmm->alloc(sizeof(phypg_t));

//     list->head->prev = list->rear;
//     list->head->next = list->rear;
//     list->rear->prev = list->head;
//     list->rear->next = list->head; //circular link list.
// }

// void phy_list_teardown(phypg_list_t *list) {
//     // assert(list->head->next == list->rear); //list can be reaped only if it's empty
//     pmm->free(list->head);
//     pmm->free(list->rear);
// }

phypg_t *alloc_page(int pgsize) {
    phypg_t *page = pmm->alloc(sizeof(phypg_t));
    page->pa = pmm->alloc(pgsize);
    page->refcnt = 1;

    assert(page->pa != NULL);
    assert(page != NULL);

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

// static void phy_list_insert(phypg_list_t *list, phypg_t *node) {
//     node->next = list->rear;
//     node->prev = list->rear->prev;
//     list->rear->prev->next = node;
//     list->rear->prev = node;
// }

// static void phy_list_remove(phypg_list_t *list, phypg_t *node) {
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

void virt_list_make(virtpg_list_t *list) {
    list->head = pmm->alloc(sizeof(phypg_t));
    list->rear = pmm->alloc(sizeof(phypg_t));

    list->head->prev = list->rear;
    list->head->next = list->rear;
    list->rear->prev = list->head;
    list->rear->next = list->head; //circular link list.
}

void virt_list_teardown(virtpg_list_t *list) {
    // assert(list->head->next == list->rear); //list can be reaped only if it's empty
    pmm->free(list->head);
    pmm->free(list->rear);
}

void virt_list_insert(virtpg_list_t *list, virtpg_t *node) {
    virtpg_t *itr = list->head->next;
    while (itr != list->rear && itr->va < node->va) {
        itr = itr->next;
    }
    assert(itr->va != node->va); //no same va;
    itr->prev->next = node;
    node->prev = itr->prev;
    node->next = itr;
    itr->prev = node;
}

void virt_list_remove(virtpg_list_t *list, virtpg_t *node) {
    printf("va=%p\n", node->va);
    panic_on(virt_list_find(list, node->va) == NULL, "Node not exists\n");

    node->prev->next = node->next;
    node->next->prev = node->prev;
    pmm->free(node);
}

virtpg_t *virt_list_find(virtpg_list_t *list, void *va) {
    virtpg_t *itr;
    for (itr = list->head->next; itr != list->rear; itr = itr->next) {
        if (itr->va == va) {
            return itr;
        }
    }
    return NULL;
}

virtpg_t *virt_node_make(void *va, phypg_t *page) {
    virtpg_t *virt_page = pmm->alloc(sizeof(virtpg_t));
    assert(virt_page != NULL);
    virt_page->va = va;
    virt_page->page = page;
    return virt_page;
}