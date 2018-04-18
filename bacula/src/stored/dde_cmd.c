/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2018 Kern Sibbald

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
#include "stored.h"


bool dde_is_unavailable(JCR *jcr)
{
   BSOCK   *dir = jcr->dir_bsock;
   dir->fsend("3900 DDE is unavailable\n");
   dir->signal(BNET_EOD);
   return true;
}

bool dedupscrub_cmd(JCR *jcr)
{
   return dde_is_unavailable(jcr);
}

bool dedupsetvar_cmd(JCR *jcr)
{
   return dde_is_unavailable(jcr);
}

bool deduptuneidxmem_cmd(JCR *jcr)
{
   return dde_is_unavailable(jcr);
}