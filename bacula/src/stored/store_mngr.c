/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 * Storage daemon's part of Storage Manager - responsible for retrieving needed
 * information in response to Director's query.
 */

#include "bacula.h"
#include "stored.h"
#include "store_mngr.h"
#include "lib/store_mngr_cmds.h"

static const int dbglvl = 200;

/* Helper Compare method for the FreeSpace policy.
 * It's purpose is to have list with unique devices. */
static int query_size_cmp(void *item1, void *item2) {
   struct query_size *qs1 = (struct query_size *) item1;
   struct query_size *qs2 = (struct query_size *) item2;

   /* We only want to avoid duplicates, order does not matter at all */
   if (qs1->devno == qs2->devno) {
      return 0;
   }

   return 1;
}

static bool query_freespace(JCR *jcr, const char *dev_name) {
   BSOCK *dir = jcr->dir_bsock;
   DEVRES *device;
   AUTOCHANGER *changer;
   uint64_t ts, fs, free_sum = 0;
   struct query_size *qs;
   alist dev_list(10, not_owned_by_alist);
   dlist size_list;
   bool found;

   /* Device sent can be either single device or an autochanger */
   foreach_res(changer, R_AUTOCHANGER) {
      if (strcmp(changer->hdr.name, dev_name) == 0) {
         /* For now we sum free size of all autochanger's unique devices and report it as a single value
          * back to the DIR in response to the FreeSpace policy querying.
          * We may want to change it at some point if we see that some other calculations/reporting make more sense  */
         foreach_alist(device, changer->device) {
            /* devno == 0 means that it's just an different directory of a device we already have */
            if (device && device->dev->devno) {
               device->dev->get_freespace(&fs, &ts);
               /* Alloc helper struct, set correct size for this dev and add it to list
                * so that later we can sum size of all unique devices */
               qs = (struct query_size *) malloc(sizeof(struct query_size));
               if (!qs) {
                  Dmsg1(dbglvl, "Failed to alloc query_size for dev: %s, policy querying stopped\n", device->device_name);
                  break;
               } else {
                  qs->size = fs;
                  qs->devno = device->dev->devno;
                  dev_list.append(qs);
               }
            }
         }

         found = true; /* No need to go through the device resource list now */
         break;
      }
   }

   if (!found) {
      /* Device sent to query is a single device instead of an autochanger */
      foreach_res(device, R_DEVICE) {
         if (strcmp(device->hdr.name, dev_name) == 0) {
            device->dev->get_freespace(&fs, &ts);
            qs = (struct query_size *) malloc(sizeof(struct query_size));
            if (!qs) {
               Dmsg1(dbglvl, "Failed to alloc query_size for dev: %s, policy querying stopped\n", device->device_name);
               break;
            } else {
               qs->size = fs;
               qs->devno = device->dev->devno;
            }
            dev_list.append(qs);
            found = true;
            break;
         }
      }
   }

   /* Add free size for each unique device */
   foreach_alist(qs, &dev_list) {
      struct query_size *tmp = (struct query_size *) size_list.binary_insert(qs, query_size_cmp);
      if (tmp) {
         free_sum+=tmp->size;
      }
   }

   dir->fsend(OK_store_size, free_sum);

   return found;
}

/* Handler for DIR's 'store_mngr query' command */
bool query_store(JCR *jcr) {
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM dev_name;
   bool ok = false;

   if (strncmp(dir->msg, store_query, strlen(store_query)) != 0) {
      return false;
   }

   dir->fsend(OK_store_query);

   /* Return parameters asked by the DIR */
   if (dir->recv() >= 0) {
      if (sscanf(dir->msg, store_query_freespace, dev_name.c_str()) == 1) {
         unbash_spaces(dev_name.c_str());

         ok = query_freespace(jcr, dev_name.c_str());
      } else {
         dir->fsend(store_query_unsupp_policy);
      }
   }

   return ok;
}
