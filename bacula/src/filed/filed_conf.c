/*
 *   Main configuration file parser for Bacula File Daemon (Client)
 *    some parts may be split into separate files such as
 *    the schedule configuration (sch_config.c).
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_config.c and 
 *	lib/parse_config.h.
 *	These files contain the parser code, some utility
 *	routines, and the common store routines (name, int,
 *	string).
 *
 *   3. The daemon specific file, which contains the Resource
 *	definitions as well as any specific store routines
 *	for the resource records.
 *
 *     Kern Sibbald, September MM
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */


#include "bacula.h"
#include "filed.h"

/* Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
int r_first = R_FIRST;
int r_last  = R_LAST;
pthread_mutex_t res_mutex =  PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced subroutines */


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
URES res_all;
int  res_all_size = sizeof(res_all);

/* Definition of records permitted within each
 * resource with the routine to process the record 
 * information.
 */ 

/* Client or File daemon "Global" resources */
static struct res_items cli_items[] = {
   {"name",     store_name,     ITEM(res_client.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,   ITEM(res_client.hdr.desc), 0, 0, 0},
   {"fdport",   store_pint,     ITEM(res_client.FDport),  0, ITEM_REQUIRED, 0},
   {"workingdirectory",  store_dir, ITEM(res_client.working_directory), 0, ITEM_REQUIRED, 0}, 
   {"piddirectory",  store_dir, ITEM(res_client.pid_directory), 0, ITEM_REQUIRED, 0}, 
   {"subsysdirectory",  store_dir,  ITEM(res_client.subsys_directory), 0, ITEM_REQUIRED, 0}, 
   {"messages",      store_res, ITEM(res_client.messages), R_MSGS, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Directors that can use our services */
static struct res_items dir_items[] = {
   {"name",        store_name,     ITEM(res_dir.hdr.name),  0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(res_dir.hdr.desc),  0, 0, 0},
   {"password",    store_password, ITEM(res_dir.password),  0, ITEM_REQUIRED, 0},
   {"address",     store_str,      ITEM(res_dir.address),   0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Message resource */
extern struct res_items msgs_items[];

/* 
 * This is the master resource definition.  
 * It must have one item for each of the resources.
 */
struct s_res resources[] = {
   {"director",      dir_items,   R_DIRECTOR,  NULL},
   {"filedaemon",    cli_items,   R_CLIENT,    NULL},
   {"client",        cli_items,   R_CLIENT,    NULL}, /* alias for filedaemon */
   {"messages",      msgs_items,  R_MSGS,      NULL},
   {NULL,	     NULL,	  0,	       NULL}
};


/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   int recurse = 1;

   if (res == NULL) {
      sendit(sock, "No record for %d %s\n", type, res_to_str(type));
      return;
   }
   if (type < 0) {		      /* no recursion */
      type = - type;
      recurse = 0;
   }
   switch (type) {
      case R_DIRECTOR:
         sendit(sock, "Director: name=%s password=%s\n", reshdr->name, 
		 res->res_dir.password);
	 break;
      case R_CLIENT:
         sendit(sock, "Client: name=%s FDport=%d\n", reshdr->name,
		 res->res_client.FDport);
	 break;
      case R_MSGS:
         sendit(sock, "Messages: name=%s\n", res->res_msgs.hdr.name);
	 if (res->res_msgs.mail_cmd) 
            sendit(sock, "      mailcmd=%s\n", res->res_msgs.mail_cmd);
	 if (res->res_msgs.operator_cmd) 
            sendit(sock, "      opcmd=%s\n", res->res_msgs.operator_cmd);
	 break;
      default:
         sendit(sock, "Unknown resource type %d\n", type);
   }
   if (recurse && res->res_dir.hdr.next)
      dump_resource(type, res->res_dir.hdr.next, sendit, sock);
}

/* 
 * Free memory of resource.  
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that 
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(int type)
{
   URES *res;
   RES *nres;
   int rindex = type - r_first;

   res = (URES *)resources[rindex].res_head;

   if (res == NULL) {
      return;
   }

   /* common stuff -- free the resource name */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name)
      free(res->res_dir.hdr.name);
   if (res->res_dir.hdr.desc)
      free(res->res_dir.hdr.desc);

   switch (type) {
      case R_DIRECTOR:
	 if (res->res_dir.password)
	    free(res->res_dir.password);
	 if (res->res_dir.address)
	    free(res->res_dir.address);
	 break;
      case R_CLIENT:
	 if (res->res_client.working_directory)
	    free(res->res_client.working_directory);
	 if (res->res_client.pid_directory)
	    free(res->res_client.pid_directory);
	 if (res->res_client.subsys_directory)
	    free(res->res_client.subsys_directory);
	 break;
      case R_MSGS:
	 if (res->res_msgs.mail_cmd)
	    free(res->res_msgs.mail_cmd);
	 if (res->res_msgs.operator_cmd)
	    free(res->res_msgs.operator_cmd);
	 free_msgs_res((MSGS *)res);  /* free message resource */
	 res = NULL;
	 break;
      default:
         printf("Unknown resource type %d\n", type);
   }
   /* Common stuff again -- free the resource, recurse to next one */
   if (res) {
      free(res);
   }
   resources[rindex].res_head = nres;
   if (nres) {
      free_resource(type);
   }
}

/* Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
void save_resource(int type, struct res_items *items, int pass)
{
   URES *res;
   int rindex = type - r_first;
   int i, size;
   int error = 0;

   /* 
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & ITEM_REQUIRED) {
	    if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {  
               Emsg2(M_ABORT, 0, _("%s item is required in %s resource, but not found.\n"),
		 items[i].name, resources[rindex]);
	     }
      }
   }

   /* During pass 2, we looked up pointers to all the resources
    * referrenced in the current resource, , now we
    * must copy their address from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
	 /* Resources not containing a resource */
	 case R_MSGS:
	 case R_DIRECTOR:
	    break;

	 /* Resources containing another resource */
	 case R_CLIENT:
	    if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ABORT, 0, "Cannot find Client resource %s\n", res_all.res_dir.hdr.name);
	    }
	    res->res_client.messages = res_all.res_client.messages;
	    break;
	 default:
            Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
	    error = 1;
	    break;
      }
      /* Note, the resoure name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_dir.hdr.name) {
	 free(res_all.res_dir.hdr.name);
	 res_all.res_dir.hdr.name = NULL;
      }
      if (res_all.res_dir.hdr.desc) {
	 free(res_all.res_dir.hdr.desc);
	 res_all.res_dir.hdr.desc = NULL;
      }
      return;
   }

   /* The following code is only executed on pass 1 */
   switch (type) {
      case R_DIRECTOR:
	 size = sizeof(DIRRES);
	 break;
      case R_CLIENT:
	 size = sizeof(CLIENT);
	 break;
      case R_MSGS:
	 size = sizeof(MSGS);
	 break;
      default:
         printf(_("Unknown resource type %d\n"), type);
	 error = 1;
	 break;
   }
   /* Common */
   if (!error) {
      res = (URES *)malloc(size);
      memcpy(res, &res_all, size);
      res->res_dir.hdr.next = resources[rindex].res_head;
      resources[rindex].res_head = (RES *)res;
      Dmsg1(90, "dir_conf: inserting res: %s\n", res->res_dir.hdr.name);
   }

}
