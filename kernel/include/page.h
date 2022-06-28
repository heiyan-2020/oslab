#include <os.h>
/*============================================
                declarations
=============================================*/

void page_list_make(phypg_list_t *list);
phypg_t *alloc_page(phypg_list_t *list, int);
void release_page(phypg_list_t *, phypg_t *);
