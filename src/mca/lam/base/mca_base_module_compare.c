/*
 * $HEADER$
 */

#include "lam_config.h"

#include <string.h>

#include "mca/mca.h"
#include "mca/lam/base/base.h"


/*
 * Function for comparing two mca_base_module_priorit_t structs so
 * that we can build prioritized LIST's of them.  This assumed that
 * the types of the modules are the same.  Sort first by priority,
 * second by module name, third by module version.
 *
 * Note that we acutally want a *reverse* ordering here -- the al_*
 * functions will put "smaller" items at the head, and "larger" items
 * at the tail.  Since we want the highest priority at the head, it
 * may help the gentle reader to consider this an inverse comparison.
 * :-)
 */
int mca_base_module_compare(mca_base_module_priority_list_item_t *a,  
                            mca_base_module_priority_list_item_t *b)
{
  int val;

  /* First, compare the priorties */

  if (a->mpli_priority > b->mpli_priority)
    return -1;
  else if (a->mpli_priority < b->mpli_priority)
    return 1;
  else {
    mca_base_module_t *aa = a->mpli_module;
    mca_base_module_t *bb = b->mpli_module;

    /* The priorities were equal, so compare the names */

    val = strncmp(aa->mca_module_name, bb->mca_module_name,
                  MCA_BASE_MAX_MODULE_NAME_LEN);
    if (val != 0)
      return -val;

    /* The names were equal, so compare the versions */

    if (aa->mca_module_major_version > bb->mca_module_major_version)
      return -1;
    else if (aa->mca_module_major_version < bb->mca_module_major_version)
      return 1;

    else if (aa->mca_module_minor_version > bb->mca_module_minor_version)
      return -1;
    else if (aa->mca_module_minor_version < bb->mca_module_minor_version)
      return 1;

    else if (aa->mca_module_release_version > bb->mca_module_release_version)
      return -1;
    else if (aa->mca_module_release_version < bb->mca_module_release_version)
      return 1;
  }

  /* They're equal */

  return 0;
}
