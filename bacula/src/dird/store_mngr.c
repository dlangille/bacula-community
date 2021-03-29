/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

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

#include "bacula.h"
#include "dird.h"

storage::storage() {
   list = New(alist(10, not_owned_by_alist));
   source = get_pool_memory(PM_MESSAGE);
   list_str = get_pool_memory(PM_MESSAGE);
   *source = 0;
   store = NULL;
   mutex = PTHREAD_MUTEX_INITIALIZER;
}

storage::~storage() {
   store = NULL;
   if (list) {
      delete list;
      list = NULL;
   }
   if (source) {
      free_and_null_pool_memory(source);
   }
   if (list_str) {
      free_and_null_pool_memory(list_str);
   }
}

void storage::set_rw(bool write) {
   P(mutex);
   this->write = write;
   V(mutex);
}

alist *storage::get_list() {
   return list;
}

const char *storage::get_source() const {
   return source;
}

const char *storage::get_media_type() const {
   return store->media_type;
}

void storage::set(STORE *storage, const char *source) {
   if (!storage) {
      return;
   }

   P(mutex);

   reset();

   list->append(storage);

   store = storage;
   if (!source) {
      pm_strcpy(this->source, _("Not specified"));
   } else {
      pm_strcpy(this->source, source);
   }

   V(mutex);
}

/* Set storage override. Remove all previous storage */
void storage::set(alist *storage, const char *source) {
   if (!storage) {
      return;
   }

   P(mutex);

   reset();

   STORE *s;
   foreach_alist(s, storage) {
      list->append(s);
   }

   store = (STORE *)list->first();
   if (!source) {
      pm_strcpy(this->source, _("Not specified"));
   } else {
      pm_strcpy(this->source, source);
   }

   V(mutex);
}

void storage::reset() {
   store = NULL;
   while (list->size()) {
      list->remove(0);
   }
   *source = 0;
   *list_str = 0;
}

/* Set custom storage for next usage (it needs to be an item from the current store list) */
bool storage::set_current_storage(STORE *storage) {
   if (!storage) {
      return false;
   }

   P(mutex);

   STORE *s;
   foreach_alist(s, list) {
      if (s == storage) {
         store = storage;
         V(mutex);
         return true;
      }
   }

   V(mutex);
   return false;
}

bool storage::inc_rstores(JCR *jcr) {
   if (list->empty()) {
      return true;
   }

   P(mutex);
   int num = store->getNumConcurrentJobs();
   int numread = store->getNumConcurrentReadJobs();
   int maxread = store->MaxConcurrentReadJobs;
   if (num < store->MaxConcurrentJobs &&
         (jcr->getJobType() == JT_RESTORE ||
          numread == 0     ||
          maxread == 0     ||     /* No limit set */
          numread < maxread))     /* Below the limit */
   {
      numread = store->incNumConcurrentReadJobs(1);
      num = store->incNumConcurrentJobs(1);
      Dmsg1(200, "Inc rncj=%d\n", num);
      V(mutex);
      return true;
   }
   V(mutex);
   return false;
}

bool storage::inc_wstores(JCR *jcr) {
   STORE *store;
   bool ret;
   if (list->empty()) {
      return true;
   }

   P(mutex);

   /* Create a temp copy of wstore list */
   alist tmp_list(10, not_owned_by_alist);
   foreach_alist(store, list) {
      tmp_list.append(store);
   }

   /* Reset write list */
   list->destroy();
   list->init(10, not_owned_by_alist);

   foreach_alist(store, &tmp_list) {
      Dmsg1(200, "Wstore=%s\n", store->name());
      int num = store->getNumConcurrentJobs();
      if (num < store->MaxConcurrentJobs) {
         num = store->incNumConcurrentJobs(1);
         Dmsg1(200, "Inc wncj=%d\n", num);
         list->append(store);
      }
   }

   if (!list->empty()) {
      ret = true;
   } else {
      /* Failed to increment counter in at least one storage */
      ret = false;
   }

   V(mutex);
   return ret;
}

bool storage::inc_stores(JCR *jcr) {
   if (write) {
      return inc_wstores(jcr);
   } else {
      return inc_rstores(jcr);
   }
}

void storage::dec_stores() {
   if (list->empty()) {
      return;
   }

   P(mutex);
   STORE *store;
   foreach_alist(store, list) {
      int num = store->incNumConcurrentJobs(-1);
      Dmsg1(200, "Dec wncj=%d\n", num);
   }
   V(mutex);
}

const char *storage::print_list() {
   P(mutex);

   *list_str = 0;
   STORE *store;
   POOL_MEM tmp;
   bool first = true;

   foreach_alist(store, list) {
      if (first) {
         first = false;
      } else {
         pm_strcat(tmp.addr(), ", ");
      }
      pm_strcat(tmp.addr(), store->name());
   }

   V(mutex);
   return quote_string(list_str, tmp.addr());
}


void LeastUsedStore::apply_policy(bool write_store) {
   alist *store = write_store ? wstore.get_list() : rstore.get_list();
   alist tmp_list(10, not_owned_by_alist);
   uint32_t store_count = store->size();
   uint32_t i, j, swap;
   //TODO arrays below limit store list to 64 items currently...
   uint32_t conc_arr[64];
   uint32_t idx_arr[64];


   for (uint32_t i=0; i<store_count; i++) {
      tmp_list.append(store->get(i));
   }

   /* Reset list */
   store->destroy();
   store->init(10, not_owned_by_alist);

   STORE *storage;
   foreach_alist_index(i, storage, &tmp_list) {
      idx_arr[i] = i;
      conc_arr[i] = storage->getNumConcurrentJobs();
   }

   /* Simple bubble sort */
   for (i = 0; i<store_count - 1; i++) {
      for (j =0; j<store_count - i -1; j++) {
         if (conc_arr[i] > conc_arr[i+1]) {
            swap = conc_arr[i];
            conc_arr[i] = conc_arr[i+1];
            conc_arr[i+1] = swap;
            swap = idx_arr[i];
            idx_arr[i] = idx_arr[i+1];
            idx_arr[i+1] = swap;
         }
      }
   }

   for (i=0; i<store_count; i++) {
      storage = (STORE *)tmp_list.get(idx_arr[i]);
      store->append(storage);
   }
}


StorageManager::StorageManager() {
   rstore.set_rw(false);
   wstore.set_rw(true);
};

STORE *StorageManager::get_rstore() {
   return rstore.get_store();
}

alist *StorageManager::get_rstore_list() {
   return rstore.get_list();
}

const char *StorageManager::get_rsource() const {
   return rstore.get_source();
}

const char *StorageManager::get_rmedia_type() const {
   return rstore.get_media_type();
}

alist *StorageManager::get_wstore_list() {
   return wstore.get_list();
}

const char *StorageManager::get_wsource() const {
   return wstore.get_source();
}

const char *StorageManager::get_wmedia_type() const {
   return wstore.get_media_type();
}

void StorageManager::set_rstore(STORE *storage, const char *source) {
   rstore.set(storage, source);
}

void StorageManager::set_rstore(alist *storage, const char *source) {
   rstore.set(storage, source);
}

void StorageManager::reset_rstorage() {
   rstore.reset();
}

const char *StorageManager::print_rlist() {
   return rstore.print_list();
}

bool StorageManager::set_current_wstorage(STORE *storage) {
   return wstore.set_current_storage(storage);
}

void StorageManager::set_wstorage(STORE *storage, const char *source) {
   wstore.set(storage, source);
}

void StorageManager::set_wstorage(alist *storage, const char *source) {
   wstore.set(storage, source);
}

void StorageManager::reset_wstorage() {
   wstore.reset();
}

const char *StorageManager::print_wlist() {
   return wstore.print_list();
}

void StorageManager::reset_rwstorage() {
   rstore.reset();
   wstore.reset();
}

bool StorageManager::inc_read_stores(JCR *jcr) {
   return rstore.inc_stores(jcr);
}

void StorageManager::dec_read_stores() {
   return rstore.dec_stores();
}

bool StorageManager::inc_write_stores(JCR *jcr) {
   return wstore.inc_stores(jcr);

}

void StorageManager::dec_write_stores() {
   return wstore.dec_stores();
}
