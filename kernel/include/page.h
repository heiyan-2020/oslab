#include <os.h>
/*============================================
                declarations
=============================================*/

void phy_list_make(phypg_list_t *list);
phypg_t *alloc_page(int);
void release_page(phypg_list_t *, phypg_t *);

void virt_list_make(virtpg_list_t *list);
void virt_list_teardown(virtpg_list_t *list);
void virt_list_insert(virtpg_list_t *list, virtpg_t *node);
void virt_list_remove(virtpg_list_t *list, virtpg_t *node);
virtpg_t *virt_list_find(virtpg_list_t *list, void *va);
virtpg_t *virt_node_make(void *va, phypg_t *page);
