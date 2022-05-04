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
 *
 *   Bacula Director -- User Agent Output Commands
 *     I.e. messages, listing database, showing resources, ...
 *
 *     Kern Sibbald, September MM
 */

#include "bacula.h"
#include "dird.h"

/* Imported subroutines */

/* Imported variables */

/* Imported functions */

/* Forward referenced functions */
static int do_list_cmd(UAContext *ua, const char *cmd, e_list_type llist);
static bool list_nextvol(UAContext *ua, int ndays);

/*
 * Turn auto display of console messages on/off
 */
int autodisplay_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      NT_("on"),
      NT_("off"),
      NULL};

   switch (find_arg_keyword(ua, kw)) {
   case 0:
      ua->auto_display_messages = true;
      break;
   case 1:
      ua->auto_display_messages = false;
      break;
   default:
      ua->error_msg(_("ON or OFF keyword missing.\n"));
      break;
   }
   return 1;
}

/*
 * Turn GUI mode on/off
 */
int gui_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      NT_("on"),
      NT_("off"),
      NULL};

   switch (find_arg_keyword(ua, kw)) {
   case 0:
      ua->jcr->gui = ua->gui = true;
      break;
   case 1:
      ua->jcr->gui = ua->gui = false;
      break;
   default:
      ua->error_msg(_("ON or OFF keyword missing.\n"));
      break;
   }
   return 1;
}

/*
 * Enter with Resources locked
 */
static void show_disabled_jobs(UAContext *ua)
{
   JOB *job;
   bool first = true;
   foreach_res(job, R_JOB) {
      if (!acl_access_ok(ua, Job_ACL, job->name())) {
         continue;
      }
      if (!job->is_enabled()) {
         if (first) {
            first = false;
            ua->send_msg(_("Disabled Jobs:\n"));
         }
         ua->send_msg("   %s\n", job->name());
     }
  }
  if (first) {
     ua->send_msg(_("No disabled Jobs.\n"));
  }
}

struct showstruct {const char *res_name; int type;};
static struct showstruct reses[] = {
   {NT_("directors"),  R_DIRECTOR},
   {NT_("clients"),    R_CLIENT},
   {NT_("counters"),   R_COUNTER},
   {NT_("devices"),    R_DEVICE},
   {NT_("jobs"),       R_JOB},
   {NT_("storages"),   R_STORAGE},
   {NT_("catalogs"),   R_CATALOG},
   {NT_("schedules"),  R_SCHEDULE},
   {NT_("filesets"),   R_FILESET},
   {NT_("pools"),      R_POOL},
   {NT_("messages"),   R_MSGS},
   {NT_("statistics"), R_COLLECTOR},
   {NT_("consoles"),   R_CONSOLE},
// {NT_("jobdefs"),    R_JOBDEFS},
// {NT_{"autochangers"), R_AUTOCHANGER},
   {NT_("all"),        -1},
   {NT_("help"),       -2},
   {NULL,           0}
};


/*
 *  Displays Resources
 *
 *  show all
 *  show <resource-keyword-name>  e.g. show directors
 *  show <resource-keyword-name>=<name> e.g. show director=HeadMan
 *  show disabled    shows disabled jobs
 *
 */
int show_cmd(UAContext *ua, const char *cmd)
{
   int i, j, type, len;
   int recurse;
   char *res_name;
   RES_HEAD *reshead = NULL;
   RES *res = NULL;

   Dmsg1(20, "show: %s\n", ua->UA_sock->msg);


   LockRes();
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("disabled")) == 0) {
         show_disabled_jobs(ua);
         goto bail_out;
      }

      res = NULL;
      reshead = NULL;
      type = 0;

      res_name = ua->argk[i];
      if (!ua->argv[i]) {             /* was a name given? */
         /* No name, dump all resources of specified type */
         recurse = 1;
         len = strlen(res_name);
         for (j=0; reses[j].res_name; j++) {
            if (strncasecmp(res_name, reses[j].res_name, len) == 0) {
               type = reses[j].type;
               if (type > 0) {
                  reshead = res_head[type-r_first];
               } else {
                  reshead = NULL;
               }
               break;
            }
         }

      } else {
         /* Dump a single resource with specified name */
         recurse = 0;
         len = strlen(res_name);
         for (j=0; reses[j].res_name; j++) {
            if (strncasecmp(res_name, reses[j].res_name, len) == 0) {
               type = reses[j].type;
               res = (RES *)GetResWithName(type, ua->argv[i]);
               if (!res) {
                  type = -3;
               }
               break;
            }
         }
      }

      switch (type) {
      /* All resources */
      case -1:
         for (j=r_first; j<=r_last; j++) {
            /* Skip R_DEVICE since it is really not used or updated */
            if (j != R_DEVICE) {
               dump_each_resource(j, bsendmsg, ua);
            }
         }
         break;
      /* Help */
      case -2:
         ua->send_msg(_("Keywords for the show command are:\n"));
         for (j=0; reses[j].res_name; j++) {
            ua->error_msg("%s\n", reses[j].res_name);
         }
         goto bail_out;
      /* Resource not found */
      case -3:
         ua->error_msg(_("%s resource %s not found.\n"), res_name, ua->argv[i]);
         goto bail_out;
      /* Resource not found */
      case 0:
         ua->error_msg(_("Resource %s not found\n"), res_name);
         goto bail_out;
      /* Dump a specific type */
      default:
         if (res) {             /* keyword and argument, ie: show job=name */
            dump_resource(recurse?type:-type, res, bsendmsg, ua);

         } else if (reshead) {  /* keyword only, ie: show job */
            dump_each_resource(-type, bsendmsg, ua);
         }
         break;
      }
   }
bail_out:
   UnlockRes();
   return 1;
}

/*
 * Check if the access is permitted for a list of jobids
 *
 * Not in ua_acl.c because it's using db access, and tools such
 * as bdirjson are not linked with cats.
 */
bool acl_access_jobid_ok(UAContext *ua, const char *jobids)
{
   char     *tmp=NULL, *p;
   bool      ret=false;
   JOB_DBR   jr;
   uint32_t  jid;

   if (!jobids) {
      return false;
   }

   if (!is_a_number_list(jobids)) {
      return false;
   }

   /* If no console resource => default console and all is permitted */
   if (!ua || !ua->cons) {
      Dmsg0(1400, "Root cons access OK.\n");
      return true;     /* No cons resource -> root console OK for everything */
   }

   alist *list = ua->cons->ACL_lists[Job_ACL];
   if (!list) {                       /* empty list */
      return false;                   /* List empty, reject everything */
   }

   /* Special case *all* gives full access */
   if (list->size() == 1 && strcasecmp("*all*", (char *)list->get(0)) == 0) {
      return true;
   }

   /* If we can't open the database, just say no */
   if (!open_new_client_db(ua)) {
      return false;
   }

   p = tmp = bstrdup(jobids);

   while (get_next_jobid_from_list(&p, &jid) > 0) {
      bmemset(&jr, 0, sizeof(jr));
      jr.JobId = jid;

      if (db_get_job_record(ua->jcr, ua->db, &jr)) {
         ret = false;
         for (int i=0; i<list->size(); i++) {
            if (strcasecmp(jr.Name, (char *)list->get(i)) == 0) {
               Dmsg3(1400, "ACL found %s in %d %s\n", jr.Name,
                     Job_ACL, (char *)list->get(i));
               ret = true;
               break;
            }
         }
         if (ret && !acl_access_client_ok(ua, jr.Client, JT_BACKUP)) {
            ret = false;
         }
      }
      if (!ret) {
         goto bail_out;
      }
   }

bail_out:
   if (tmp) {
      free(tmp);
   }
   return ret;
}

/*
 *  List contents of database
 *
 *  list jobs           - lists all jobs run
 *  list jobid=nnn      - list job data for jobid
 *  list ujobid=uname   - list job data for unique jobid
 *  list job=name       - list all jobs with "name"
 *  list jobname=name   - same as above
 *  list jobmedia jobid=<nn>
 *  list jobmedia job=name
 *  list joblog pattern=xxx jobid=<nn> 
 *  list joblog pattern=xxx jobid=<nn> 
 *  list joblog job=name
 *  list files [type=<deleted|all|malware>] jobid=<nn> - list files saved for job nn
 *  list files [type=<deleted|all|malware>] job=name
 *  list pools          - list pool records
 *  list jobtotals      - list totals for all jobs
 *  list media          - list media for given pool (deprecated)
 *  list volumes        - list Volumes
 *  list clients        - list clients
 *  list nextvol job=xx  - list the next vol to be used by job
 *  list nextvolume job=xx - same as above.
 *  list copies jobid=x,y,z
 *  list objects [type=objecttype job_id=id clientname=n,status=S] - list plugin objects
 *  list pluginrestoreconf jobid=x,y,z [id=k]
 *  list filemedia jobid=x fileindex=z
 *  list metadata type=[email|attachment] tenant=xxx owner=xxx jobid=<x,w,z> client=<cli>
 *             from=<str>
 *             to=<str> cc=<str> tags=<str> 
 *             subject=<str> bodypreview=<str> all=<str> minsize=<int> maxsize=<int> 
 *             importance=<str> isread=<0|1> isdraft=<0|1>
 *             categories=<str> conversationid=<str> hasattachment=<0|1>
 *             starttime=<time> endtime=<time>
 *             limit=<int> offset=<int>  order=<Asc|desc> alljobs
 *             emailid=<str>
 *  Note: keyword "long" is before the first command on the command 
 *    line results in doing a llist (long listing).
 */

/* Do long or full listing */
bool jlist_cmd(UAContext *ua, const char *cmd)
{
   return do_list_cmd(ua, cmd, JSON_LIST);
}

int llist_cmd(UAContext *ua, const char *cmd)
{
   return do_list_cmd(ua, cmd, VERT_LIST);
}

/* Do short or summary listing */
int list_cmd(UAContext *ua, const char *cmd)
{
   if (find_arg(ua, "long") > 0) {
      return do_list_cmd(ua, cmd, VERT_LIST);  /* do a long list */
   } else {
      return do_list_cmd(ua, cmd, HORZ_LIST);  /* do a short list */
   }
}

/* Simple helper to setup date used for filtering records.
 * Value can be passed as a days or hours count */
static bool setup_start_date(const char *val, bool days, char *dest, uint32_t dest_len)
{
   time_t stime;
   struct tm tm;
   int hours_count;

   if (!is_an_integer(val)) {
      return false;
   }

   /* Do we have hours or days count */
   hours_count = days ? 24 : 1;
   stime = time(NULL) - atoi(val) * hours_count * 60*60;
   (void)localtime_r(&stime, &tm);
   strftime(dest, dest_len, "%Y-%m-%d %H:%M:%S", &tm);

   return true;
}

/* shortcuts for the list object category=xx command */
struct object_category_sc_t {
   const char *name;
   const char *value;
};

static
struct object_category_sc_t object_category_sc[] = {
   NT_("db"), NT_("Database"),
   NT_("vm"), NT_("Virtual Machine"),
   NULL, NULL
};

static int do_list_cmd(UAContext *ua, const char *cmd, e_list_type llist)
{
   POOLMEM *VolumeName;
   int jobid=0, n;
   int i, j;
   JOB_DBR jr;
   POOL_DBR pr;
   MEDIA_DBR mr;

   if (!open_new_client_db(ua)) {
      return 1;
   }

   bmemset(&jr, 0, sizeof(jr));
   bmemset(&pr, 0, sizeof(pr));

   Dmsg1(20, "list: %s\n", cmd);

   if (!ua->db) {
      ua->error_msg(_("Hey! DB is NULL\n"));
   }
   /* Apply any limit */
   for (j = 1; j < ua->argc ; j++) {
      if (strcasecmp(ua->argk[j], NT_("joberrors")) == 0) {
         jr.JobErrors = 1;
      } else if (!ua->argv[j]) {
         /* skip */
       } else if (strcasecmp(ua->argk[j], NT_("order")) == 0) {
         if (strcasecmp(ua->argv[j], NT_("desc")) == 0 ||
             strcasecmp(ua->argv[j], NT_("descending")) == 0) {
             jr.order = 1;

         } else if (strcasecmp(ua->argv[j], NT_("asc")) == 0 ||
                    strcasecmp(ua->argv[j], NT_("ascending")) == 0) {
            jr.order = 0;

         } else {
            ua->error_msg(_("Unknown order type %s\n"), ua->argv[j]);
            return 1;
         }
      } else if (strcasecmp(ua->argk[j], NT_("limit")) == 0) {
         jr.limit = atoi(ua->argv[j]);

      } else if (strcasecmp(ua->argk[j], NT_("jobstatus")) == 0) {
         if (B_ISALPHA(ua->argv[j][0])) {
            jr.JobStatus = ua->argv[j][0]; /* TODO: Check if the code is correct */
         }
      } else if (strcasecmp(ua->argk[j], NT_("jobtype")) == 0) {
         if (B_ISALPHA(ua->argv[j][0])) {
            jr.JobType = ua->argv[j][0]; /* TODO: Check if the code is correct */
         }
      } else if (strcasecmp(ua->argk[j], NT_("level")) == 0) {
         if (strlen(ua->argv[j]) > 1) {
            jr.JobLevel = get_level_code_from_name(ua->argv[j]);

         } else if (B_ISALPHA(ua->argv[j][0])) {
            jr.JobLevel = ua->argv[j][0]; /* TODO: Check if the code is correct */
         }
      } else if (strcasecmp(ua->argk[j], NT_("client")) == 0) {
         if (is_name_valid(ua->argv[j], NULL)) {
            CLIENT_DBR cr;
            bmemset(&cr, 0, sizeof(cr));
            /* Both Backup & Restore wants to list jobs for this client */
            if(get_client_dbr(ua, &cr, JT_BACKUP_RESTORE)) {
               jr.ClientId = cr.ClientId;
            }
         }
      } else if (strcasecmp(ua->argk[j], NT_("days")) == 0 && ua->argv[j]) {
         if (!setup_start_date(ua->argv[j], true, jr.FromDate, sizeof(jr.FromDate))) {
            ua->error_msg(_("Expected integer passed as a days count, got: %s\n"), ua->argv[j]);
            return 1;
         }
      } else if (strcasecmp(ua->argk[j], NT_("hours")) == 0 && ua->argv[j]) {
         if (!setup_start_date(ua->argv[j], false, jr.FromDate, sizeof(jr.FromDate))) {
            ua->error_msg(_("Expected integer passed as a hours count, got: %s\n"), ua->argv[j]);
            return 1;
         }
      }
   }

   /* Scan arguments looking for things to do */
   for (i=1; i<ua->argc; i++) {
      /* List JOBS */
      if (strcasecmp(ua->argk[i], NT_("jobs")) == 0) {
         db_list_job_records(ua->jcr, ua->db, &jr, prtit, ua, llist);

         /* List JOBTOTALS */
      } else if (strcasecmp(ua->argk[i], NT_("jobtotals")) == 0) {
         db_list_job_totals(ua->jcr, ua->db, &jr, prtit, ua);

      /* List JOBID=nn */
      } else if (strcasecmp(ua->argk[i], NT_("jobid")) == 0) {
         if (ua->argv[i]) {
            /* .jlist joblog jobid=1   should display only one json struct */
            if (llist == JSON_LIST && i > 1) {
               /* nop */
            } else {
               jobid = str_to_int64(ua->argv[i]);
               if (jobid > 0) {
                  jr.JobId = jobid;
                  db_list_job_records(ua->jcr, ua->db, &jr, prtit, ua, llist);
               }
            }
         }

      /* List JOB=xxx */
      } else if ((strcasecmp(ua->argk[i], NT_("job")) == 0 ||
                  strcasecmp(ua->argk[i], NT_("jobname")) == 0) && ua->argv[i]) {
         bstrncpy(jr.Name, ua->argv[i], MAX_NAME_LENGTH);
         jr.JobId = 0;
         db_list_job_records(ua->jcr, ua->db, &jr, prtit, ua, llist);

      /* List UJOBID=xxx */
      } else if (strcasecmp(ua->argk[i], NT_("ujobid")) == 0 && ua->argv[i]) {
         bstrncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
         jr.JobId = 0;
         db_list_job_records(ua->jcr, ua->db, &jr, prtit, ua, llist);

      /* List Base files */
      } else if (strcasecmp(ua->argk[i], NT_("basefiles")) == 0) {
         /* TODO: cleanup this block */
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j]) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;
               db_get_job_record(ua->jcr, ua->db, &jr);
               jobid = jr.JobId;
            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);
            } else {
               continue;
            }
            if (jobid > 0) {
               db_list_base_files_for_job(ua->jcr, ua->db, jobid, prtit, ua);
            }
         }

      /* List FILES */
      } else if (strcasecmp(ua->argk[i], NT_("files")) == 0) {
         int deleted = 0;       /* see only backed up files */
         char malware = 0;       /* List malware detected */
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j]) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;
               db_get_job_record(ua->jcr, ua->db, &jr);
               jobid = jr.JobId;

            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("type")) == 0 && ua->argv[j]) {
               if (strcasecmp(ua->argv[j], NT_("deleted")) == 0) {
                  deleted = 1;
               } else if (strcasecmp(ua->argv[j], NT_("all")) == 0) {
                  deleted = -1;
               } else if (strcasecmp(ua->argv[j], NT_("malware")) == 0) {
                  malware = 'M';
               }
               continue;        /* Type should be before the jobid... */
            } else {
               continue;
            }
            if (jobid > 0) {
               if (malware) {
                  db_list_fileevents_for_job(ua->jcr, ua->db, jobid, malware, prtit, ua, llist);

               } else {
                  db_list_files_for_job(ua->jcr, ua->db, jobid, deleted, prtit, ua);
               }
            }
         }

      /* List JOBMEDIA */
      } else if (strcasecmp(ua->argk[i], NT_("jobmedia")) == 0) {
         char *volume = NULL;
         jobid = 0;

         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j] && is_name_valid(ua->argv[j], NULL)) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;
               db_get_job_record(ua->jcr, ua->db, &jr);
               jobid = jr.JobId;

            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("volume")) == 0 &&
                       ua->argv[j] && is_volume_name_legal(ua, ua->argv[j])) {
               volume = ua->argv[j];

            } else {
               continue;
            }
         }

         db_list_jobmedia_records(ua->jcr, ua->db, jobid, volume, prtit, ua, llist);
         return 1;

      /* list filemedia */
      } else if (strcasecmp(ua->argk[i], NT_("filemedia")) == 0) {
         int32_t findex=0;
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("fileindex")) == 0 && ua->argv[j]) {
               findex = str_to_int64(ua->argv[i]);

            } else {
               continue;
            }
         }
         if (jobid) {
            db_list_filemedia_records(ua->jcr, ua->db, jobid, findex, prtit, ua, llist);
         }
         return 1;

      /* List JOBLOG */
      } else if (strcasecmp(ua->argk[i], NT_("joblog")) == 0) {
         bool done = false;
         const char *pattern=NULL;
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j]) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;
               db_get_job_record(ua->jcr, ua->db, &jr);
               jobid = jr.JobId;

            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("pattern")) == 0 && ua->argv[j]) {
               pattern = ua->argv[j];
               continue;

            } else {
               continue;
            }
            db_list_joblog_records(ua->jcr, ua->db, jobid, pattern, prtit, ua, llist);
            done = true;
         }
         if (!done) {
            /* List for all jobs (jobid=0) */
            db_list_joblog_records(ua->jcr, ua->db, 0, pattern, prtit, ua, llist);
         }

      /* List POOLS */
      } else if (strcasecmp(ua->argk[i], NT_("pool")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("pools")) == 0) {
         POOL_DBR pr;
         bmemset(&pr, 0, sizeof(pr));
         if (ua->argv[i]) {
            bstrncpy(pr.Name, ua->argv[i], sizeof(pr.Name));
         }
         db_list_pool_records(ua->jcr, ua->db, &pr, prtit, ua, llist);

      } else if (strcasecmp(ua->argk[i], NT_("clients")) == 0) {
         db_list_client_records(ua->jcr, ua->db, prtit, ua, llist);

      } else if (strcasecmp(ua->argk[i], NT_("pluginrestoreconf")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("restoreobjects")) == 0)
      {
         ROBJECT_DBR rr;
         bmemset(&rr, 0, sizeof(rr));
         rr.FileType = FT_PLUGIN_CONFIG;

         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j]) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;

            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {

               if (acl_access_jobid_ok(ua, ua->argv[j])) {

                  if (is_a_number(ua->argv[j])) {
                     rr.JobId = str_to_uint64(ua->argv[j]);

                  } else if (is_a_number_list(ua->argv[j])) {
                     /* In this case, loop directly to find if all jobids are
                      * accessible */
                     rr.JobIds = ua->argv[j];
                  }

               } else {
                  ua->error_msg(_("Invalid jobid argument\n"));
                  return 1;
               }

            } else if (((strcasecmp(ua->argk[j], NT_("id")) == 0) ||
                        (strcasecmp(ua->argk[j], NT_("restoreobjectid")) == 0))
                       && ua->argv[j])
            {
               rr.RestoreObjectId = str_to_uint64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("objecttype")) == 0 && ua->argv[j]) {
               if (strcasecmp(ua->argv[j], NT_("PLUGIN_CONFIG")) == 0) {
                  rr.FileType = FT_PLUGIN_CONFIG;

               } else if (strcasecmp(ua->argv[j], NT_("PLUGIN_CONFIG_FILLED")) == 0) {
                  rr.FileType = FT_PLUGIN_CONFIG_FILLED;

               } else if (strcasecmp(ua->argv[j], NT_("RESTORE_FIRST")) == 0) {
                  rr.FileType = FT_RESTORE_FIRST;

               } else if (strcasecmp(ua->argv[j], NT_("SECURITY")) == 0) {
                  rr.FileType = FT_SECURITY_OBJECT;

               } else if (strcasecmp(ua->argv[j], NT_("ALL")) == 0) {
                  rr.FileType = 0;

               } else if (is_a_number(ua->argv[j])) {
                  rr.FileType = str_to_uint64(ua->argv[j]);
                  
               } else {
                  ua->error_msg(_("Unknown ObjectType %s\n"), ua->argv[j]);
                  return 1;
               }

            } else {
               continue;
            }
         }

         if (!rr.JobId && !rr.JobIds) {
            ua->error_msg(_("list pluginrestoreconf requires jobid argument\n"));
            return 1;
         }

          /* Display the content of the restore object */
         if (rr.RestoreObjectId > 0) {
            /* Here, the JobId and the RestoreObjectId are set */
            if (db_get_restoreobject_record(ua->jcr, ua->db, &rr)) {
               ua->send_msg("%s\n", NPRTB(rr.object));
            } else {
               Dmsg0(200, "Object not found\n");
            }

         } else {
            db_list_restore_objects(ua->jcr, ua->db, &rr, prtit, ua, llist);
         }

         db_free_restoreobject_record(ua->jcr, &rr);
         return 1;

      /* List PLUGIN OBJECTS */
      } else if (strcasecmp(ua->argk[i], NT_("object")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("objects")) == 0)
      {
         OBJECT_DBR obj_r;

         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               if (is_a_number_list(ua->argv[j]) && acl_access_jobid_ok(ua, ua->argv[j])) {
                  pm_strcpy(obj_r.JobIds, ua->argv[j]);
                } else {
                  ua->error_msg(_("Invalid jobid argument\n"));
                  return 1;
               }

            } else if ((strcasecmp(ua->argk[j], NT_("objectid")) == 0) && ua->argv[j]) {
               if (is_a_number(ua->argv[j])) {
                  obj_r.ObjectId = str_to_uint64(ua->argv[j]);
               } else {
                  ua->error_msg(_("Invalid objectid argument\n"));
                  return 1;
               }

            } else if (strcasecmp(ua->argk[j], NT_("client")) == 0 && ua->argv[j]) {
               if (!acl_access_ok(ua, Client_ACL, ua->argk[j])) {
                  ua->error_msg(_("Access to Client=%s not authorized.\n"), ua->argv[j]);
                  return 0;
               }
               bstrncpy(obj_r.ClientName, ua->argv[j], sizeof(obj_r.ClientName));

            } else if (strcasecmp(ua->argk[j], NT_("name")) == 0 && ua->argv[j]) {
               bstrncpy(obj_r.ObjectName, ua->argv[j], sizeof(obj_r.ObjectName));

            } else if (strcasecmp(ua->argk[j], NT_("type")) == 0 && ua->argv[j]) {
               bstrncpy(obj_r.ObjectType, ua->argv[j], sizeof(obj_r.ObjectType));

            } else if (strcasecmp(ua->argk[j], NT_("category")) == 0 && ua->argv[j]) {
               const char *val = ua->argv[j];
               for (int i=0 ; object_category_sc[i].name ; i++) {
                  if (strcasecmp(val, object_category_sc[i].name) == 0) {
                     val = object_category_sc[i].value;
                     break;
                  }
               }
               bstrncpy(obj_r.ObjectCategory, val, sizeof(obj_r.ObjectCategory));

            } else if (strcasecmp(ua->argk[j], NT_("status")) == 0 && ua->argv[j]) {
               int32_t status = (int32_t)ua->argv[j][0];
               if ((status >= 'a' && status <= 'z') ||
                   (status >= 'A' && status <= 'Z')) {
                  obj_r.ObjectStatus = (int32_t)ua->argv[j][0];
               } else {
                  ua->error_msg(_("Invalid status argument\n"));
                  return 1;
               }
            } else if (strcasecmp(ua->argk[j], NT_("limit")) == 0 && ua->argv[j]) {
               obj_r.limit = atoi(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("order")) == 0 && ua->argv[j]) {
               /* Other order are tested before */
               obj_r.order = bstrcasecmp(ua->argv[j], "DESC") == 0;
            }
         }

         db_list_plugin_objects(ua->jcr, ua->db, &obj_r, prtit, ua, llist);
         return 1;

      /* List MEDIA or VOLUMES */
      } else if (strcasecmp(ua->argk[i], NT_("media")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("volume")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("volumes")) == 0) {
         bool done = false;
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("ujobid")) == 0 && ua->argv[j]) {
               bstrncpy(jr.Job, ua->argv[j], MAX_NAME_LENGTH);
               jr.JobId = 0;
               db_get_job_record(ua->jcr, ua->db, &jr);
               jobid = jr.JobId;
            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               jobid = str_to_int64(ua->argv[j]);
            } else {
               continue;
            }
            VolumeName = get_pool_memory(PM_FNAME);
            n = db_get_job_volume_names(ua->jcr, ua->db, jobid, &VolumeName);
            ua->send_msg(_("Jobid %d used %d Volume(s): %s\n"), jobid, n, VolumeName);
            free_pool_memory(VolumeName);
            done = true;
         }

         /* if no job or jobid keyword found, then we list all media */
         if (!done) {
            int num_pools;
            uint32_t *ids;
            /* List a specific volume? */
            if (ua->argv[i] && *ua->argv[i]) {
               bstrncpy(mr.VolumeName, ua->argv[i], sizeof(mr.VolumeName));
               db_list_media_records(ua->jcr, ua->db, &mr, prtit, ua, llist);
               return 1;
            }
            /* Is a specific pool wanted? */
            for (i=1; i<ua->argc; i++) {
               if (strcasecmp(ua->argk[i], NT_("pool")) == 0) {
                  if (!get_pool_dbr(ua, &pr)) {
                     ua->error_msg(_("No Pool specified.\n"));
                     return 1;
                  }
                  mr.PoolId = pr.PoolId;
                  db_list_media_records(ua->jcr, ua->db, &mr, prtit, ua, llist);
                  return 1;
               }
            }

            /* List Volumes in all pools */
            if (!db_get_pool_ids(ua->jcr, ua->db, &num_pools, &ids)) {
               ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"),
                        db_strerror(ua->db));
               return 1;
            }
            if (num_pools <= 0) {
               return 1;
            }
            for (i=0; i < num_pools; i++) {
               pr.PoolId = ids[i];
               if (db_get_pool_record(ua->jcr, ua->db, &pr)) {
                  ua->send_msg(_("Pool: %s\n"), pr.Name);
               }
               mr.PoolId = ids[i];
               db_list_media_records(ua->jcr, ua->db, &mr, prtit, ua, llist);
            }
            free(ids);
            return 1;
         }
      /* List next volume */
      } else if (strcasecmp(ua->argk[i], NT_("nextvol")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("nextvolume")) == 0) {
         n = 1;
         j = find_arg_with_value(ua, NT_("days"));
         if (j >= 0) {
            n = atoi(ua->argv[j]);
            if ((n < 0) || (n > 50)) {
              ua->warning_msg(_("Ignoring invalid value for days. Max is 50.\n"));
              n = 1;
            }
         }
         list_nextvol(ua, n);
      } else if (strcasecmp(ua->argk[i], NT_("copies")) == 0) {
         char *jobids = NULL;
         uint32_t limit=0;
         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("jobid")) == 0 && ua->argv[j]) {
               if (is_a_number_list(ua->argv[j])) {
                  jobids = ua->argv[j];
               }
            } else if (strcasecmp(ua->argk[j], NT_("limit")) == 0 && ua->argv[j]) {
               limit = atoi(ua->argv[j]);
            }
         }
         db_list_copies_records(ua->jcr,ua->db,limit,jobids,prtit,ua,llist);

      } else if (strcasecmp(ua->argk[i], NT_("events")) == 0) {
         EVENTS_DBR event;

         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("type")) == 0 && ua->argv[j]) {
               bstrncpy(event.EventsType, ua->argv[j], sizeof(event.EventsType));

            } else if (strcasecmp(ua->argk[j], NT_("limit")) == 0 && ua->argv[j]) {
               event.limit = atoi(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("offset")) == 0 && ua->argv[j]) {
               event.offset = atoi(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("order")) == 0 && ua->argv[j]) {
               /* Other order are tested before */
               event.order = bstrcasecmp(ua->argv[j], "DESC");

            } else if (strcasecmp(ua->argk[j], NT_("days")) == 0 && ua->argv[j]) {
               if (!setup_start_date(ua->argv[j], true, event.start, sizeof(event.start))) {
                  ua->error_msg(_("Expected integer passed as a days count, got: %s\n"), ua->argv[j]);
                  return 1;
               }
            } else if (strcasecmp(ua->argk[j], NT_("hours")) == 0 && ua->argv[j]) {
               if (!setup_start_date(ua->argv[j], false, event.start, sizeof(event.start))) {
                  ua->error_msg(_("Expected integer passed as a hours count, got: %s\n"), ua->argv[j]);
                  return 1;
               }
            } else if (strcasecmp(ua->argk[j], NT_("start")) == 0 && ua->argv[j]) {
               bstrncpy(event.start, ua->argv[j], sizeof(event.start)); /* TODO: check format */

            } else if (strcasecmp(ua->argk[j], NT_("end")) == 0 && ua->argv[j]) {
               bstrncpy(event.end, ua->argv[j], sizeof(event.end)); /* TODO: check format */

            } else if (strcasecmp(ua->argk[j], NT_("source")) == 0 && ua->argv[j]) {
               bstrncpy(event.EventsSource, ua->argv[j], sizeof(event.EventsSource)); /* TODO: check format */

            } else if (strcasecmp(ua->argk[j], NT_("daemon")) == 0 && ua->argv[j]) {
               bstrncpy(event.EventsDaemon, ua->argv[j], sizeof(event.EventsDaemon)); /* TODO: check format */

            } else if (strcasecmp(ua->argk[j], NT_("code")) == 0 && ua->argv[j]) {
               bstrncpy(event.EventsCode, ua->argv[j], sizeof(event.EventsCode)); /* TODO: check format */
            }
         }
         db_list_events_records(ua->jcr,ua->db, &event, prtit, ua, llist);
         return 1;

      } else if (strcasecmp(ua->argk[i], NT_("tag")) == 0) {
         return tag_cmd(ua, cmd);

      /* List Emails/Attachments */
      } else if (strcasecmp(ua->argk[i], NT_("metadata")) == 0) {
         META_DBR meta_r;

         for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], NT_("alljobs")) == 0) {
               meta_r.alljobs = 1;
            } else if (!ua->argv[j]) {
               ua->error_msg(_("Invalid %s argument. Expecting value\n"), ua->argk[j]);
               return 1;
            } else if (strcasecmp(ua->argk[j], NT_("jobid")) == 0) {
               if (is_a_number_list(ua->argv[j]) && acl_access_jobid_ok(ua, ua->argv[j])) {
                  meta_r.JobIds = ua->argv[j];
                } else {
                  ua->error_msg(_("Invalid jobid argument\n"));
                  return 1;
               }

            } else if (strcasecmp(ua->argk[j], NT_("client")) == 0) {
               if (!acl_access_ok(ua, Client_ACL, ua->argk[j])) {
                  ua->error_msg(_("Access to Client=%s not authorized.\n"), ua->argv[j]);
                  return 0;
               }
               bstrncpy(meta_r.ClientName, ua->argv[j], sizeof(meta_r.ClientName));

            } else if (strcasecmp(ua->argk[j], NT_("from")) == 0) {
               bstrncpy(meta_r.From, ua->argv[j], sizeof(meta_r.From));

            } else if (strcasecmp(ua->argk[j], NT_("contenttype")) == 0) {
               bstrncpy(meta_r.ContentType, ua->argv[j], sizeof(meta_r.ContentType));

            } else if (strcasecmp(ua->argk[j], NT_("name")) == 0) {
               bstrncpy(meta_r.Name, ua->argv[j], sizeof(meta_r.Name));

            } else if (strcasecmp(ua->argk[j], NT_("emailid")) == 0) {
               bstrncpy(meta_r.Id, ua->argv[j], sizeof(meta_r.Id));

            } else if (strcasecmp(ua->argk[j], NT_("to")) == 0) {
               bstrncpy(meta_r.To, ua->argv[j], sizeof(meta_r.To));

            } else if (strcasecmp(ua->argk[j], NT_("foldername")) == 0) {
               bstrncpy(meta_r.FolderName, ua->argv[j], sizeof(meta_r.FolderName));

            } else if (strcasecmp(ua->argk[j], NT_("cc")) == 0) {
               bstrncpy(meta_r.Cc, ua->argv[j], sizeof(meta_r.Cc));

            } else if (strcasecmp(ua->argk[j], NT_("tags")) == 0) {
               bstrncpy(meta_r.Tags, ua->argv[j], sizeof(meta_r.Tags));

            } else if (strcasecmp(ua->argk[j], NT_("subject")) == 0) {
               bstrncpy(meta_r.Subject, ua->argv[j], sizeof(meta_r.Subject));

            } else if (strcasecmp(ua->argk[j], NT_("bodypreview")) == 0) {
               bstrncpy(meta_r.BodyPreview, ua->argv[j], sizeof(meta_r.BodyPreview));

            } else if (strcasecmp(ua->argk[j], NT_("type")) == 0) {
               bstrncpy(meta_r.Type, ua->argv[j], sizeof(meta_r.Type));

            } else if (strcasecmp(ua->argk[j], NT_("owner")) == 0) {
               bstrncpy(meta_r.Owner, ua->argv[j], sizeof(meta_r.Owner));

            } else if (strcasecmp(ua->argk[j], NT_("tenant")) == 0) {
               bstrncpy(meta_r.Tenant, ua->argv[j], sizeof(meta_r.Tenant));

            } else if (strcasecmp(ua->argk[j], NT_("conversationid")) == 0) {
               bstrncpy(meta_r.ConversationId, ua->argv[j], sizeof(meta_r.ConversationId));

            } else if (strcasecmp(ua->argk[j], NT_("category")) == 0) {
               bstrncpy(meta_r.Category, ua->argv[j], sizeof(meta_r.Category));

            } else if (strcasecmp(ua->argk[j], NT_("all")) == 0) {
               bstrncpy(meta_r.Tags, ua->argv[j], sizeof(meta_r.Tags));
               bstrncpy(meta_r.From, ua->argv[j], sizeof(meta_r.From));
               bstrncpy(meta_r.To, ua->argv[j], sizeof(meta_r.To));
               bstrncpy(meta_r.Cc, ua->argv[j], sizeof(meta_r.Cc));
               bstrncpy(meta_r.Subject, ua->argv[j], sizeof(meta_r.Subject));
               bstrncpy(meta_r.BodyPreview, ua->argv[j], sizeof(meta_r.BodyPreview));
               bstrncpy(meta_r.Category, ua->argv[j], sizeof(meta_r.Category));
               meta_r.all = true;

            } else if (strcasecmp(ua->argk[j], NT_("minsize")) == 0) {
               uint64_t v;
               if (size_to_uint64(ua->argv[j], strlen(ua->argv[j]), &v)) {
                  meta_r.MinSize = v;
               }

            } else if (strcasecmp(ua->argk[j], NT_("maxsize")) == 0) {
               uint64_t v;
               if (size_to_uint64(ua->argv[j], strlen(ua->argv[j]), &v)) {
                  meta_r.MaxSize = v;
               }

            } else if (strcasecmp(ua->argk[j], NT_("mintime")) == 0) {
               bstrncpy(meta_r.MinTime, ua->argv[j], sizeof(meta_r.MinTime));

            } else if (strcasecmp(ua->argk[j], NT_("maxtime")) == 0) {
               bstrncpy(meta_r.MaxTime, ua->argv[j], sizeof(meta_r.MaxTime));

            } else if (strcasecmp(ua->argk[j], NT_("isread")) == 0) {
               meta_r.isRead = str_to_uint64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("isinline")) == 0) {
               meta_r.isInline = str_to_uint64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("isdraft")) == 0) {
               meta_r.isDraft = str_to_uint64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("hasattachment")) == 0) {
               meta_r.HasAttachment = str_to_uint64(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("offset")) == 0) {
               meta_r.offset = atoi(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("limit")) == 0) {
               meta_r.limit = atoi(ua->argv[j]);

            } else if (strcasecmp(ua->argk[j], NT_("orderby")) == 0) {
               meta_r.orderby = bstrcasecmp(ua->argv[j], "Time") == 1;

            } else if (strcasecmp(ua->argk[j], NT_("order")) == 0) {
               /* Other order are tested before */
               meta_r.order = bstrcasecmp(ua->argv[j], "DESC");
            }
         }
         if (!meta_r.check()) {
            ua->error_msg(_("Invalid parameters. %s\n"), meta_r.errmsg);
            return 1;
         }
         db_list_metadata_records(ua->jcr, ua->db, &meta_r, prtit, ua, llist);
         return 1;

      } else if (strcasecmp(ua->argk[i], NT_("limit")) == 0
                 || strcasecmp(ua->argk[i], NT_("days")) == 0
                 || strcasecmp(ua->argk[i], NT_("hours")) == 0
                 || strcasecmp(ua->argk[i], NT_("joberrors")) == 0
                 || strcasecmp(ua->argk[i], NT_("order")) == 0
                 || strcasecmp(ua->argk[i], NT_("jobstatus")) == 0
                 || strcasecmp(ua->argk[i], NT_("client")) == 0
                 || strcasecmp(ua->argk[i], NT_("type")) == 0
                 || strcasecmp(ua->argk[i], NT_("level")) == 0
                 || strcasecmp(ua->argk[i], NT_("jobtype")) == 0
                 || strcasecmp(ua->argk[i], NT_("long")) == 0
                 || strcasecmp(ua->argk[i], NT_("start")) == 0
                 || strcasecmp(ua->argk[i], NT_("end")) == 0
                 || strcasecmp(ua->argk[i], NT_("objecttype")) == 0
                 || strcasecmp(ua->argk[i], NT_("objectid")) == 0
                 || strcasecmp(ua->argk[i], NT_("clientname")) == 0
                 || strcasecmp(ua->argk[i], NT_("jobid")) == 0
                 || strcasecmp(ua->argk[i], NT_("source")) == 0
                 || strcasecmp(ua->argk[i], NT_("daemon")) == 0
                 || strcasecmp(ua->argk[i], NT_("code")) == 0
                 || strcasecmp(ua->argk[i], NT_("offset")) == 0
                 || strcasecmp(ua->argk[i], NT_("pattern")) == 0
         ) {
         /* Ignore it */
      } else if (strcasecmp(ua->argk[i], NT_("snapshot")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("snapshots")) == 0) 
      {
         snapshot_list(ua, i, prtit, llist);
         return 1;

      } else {
         ua->error_msg(_("Unknown list keyword: %s\n"), NPRT(ua->argk[i]));
      }
   }
   return 1;
}

static bool list_nextvol(UAContext *ua, int ndays)
{
   JOB *job;
   JCR *jcr;
   USTORE store;
   RUN *run;
   utime_t runtime;
   bool found = false;
   MEDIA_DBR mr;
   POOL_DBR pr;
   POOL_MEM errmsg;
   char edl[50];

   int i = find_arg_with_value(ua, "job");
   if (i <= 0) {
      if ((job = select_job_resource(ua)) == NULL) {
         return false;
      }
   } else {
      job = (JOB *)GetResWithName(R_JOB, ua->argv[i]);
      if (!job) {
         Jmsg(ua->jcr, M_ERROR, 0, _("%s is not a job name.\n"), ua->argv[i]);
         if ((job = select_job_resource(ua)) == NULL) {
            return false;
         }
      }
   }

   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   for (run=NULL; (run = find_next_run(run, job, runtime, ndays)); ) {
      if (!complete_jcr_for_job(jcr, job, run->pool)) {
         found = false;
         goto get_out;
      }
      if (!jcr->jr.PoolId) {
         ua->error_msg(_("Could not find Pool for Job %s\n"), job->name());
         continue;
      }
      bmemset(&pr, 0, sizeof(pr));
      pr.PoolId = jcr->jr.PoolId;
      if (!db_get_pool_record(jcr, jcr->db, &pr)) {
         bstrncpy(pr.Name, "*UnknownPool*", sizeof(pr.Name));
      }
      mr.PoolId = jcr->jr.PoolId;
      get_job_storage(&store, job, run);
      set_storageid_in_mr(store.store, &mr);
      /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
      if (!find_next_volume_for_append(jcr, &mr, 1, fnv_no_create_vol, fnv_prune, errmsg)) {
         ua->error_msg(_("Could not find next Volume for Job %s (Pool=%s, Level=%s).%s\n"),
                       job->name(), pr.Name, level_to_str(edl, sizeof(edl), run->level), errmsg.c_str());
      } else {
         ua->send_msg(
            _("The next Volume to be used by Job \"%s\" (Pool=%s, Level=%s) will be %s\n"),
            job->name(), pr.Name, level_to_str(edl, sizeof(edl), run->level),
            mr.VolumeName);
         found = true;
      }
   }

get_out:
   if (jcr->db) db_close_database(jcr, jcr->db);
   jcr->db = NULL;
   free_jcr(jcr);
   if (!found) {
      ua->error_msg(_("Could not find next Volume for Job %s.\n"),
         job->hdr.name);
      return false;
   }
   return true;
}


/*
 * For a given job, we examine all his run records
 *  to see if it is scheduled today or tomorrow.
 */
RUN *find_next_run(RUN *run, JOB *job, utime_t &runtime, int ndays)
{
   time_t now, future, endtime;
   SCHED *sched;
   struct tm tm, runtm;
   int mday, wday, month, wom, i;
   int woy, ldom;
   int day;
   bool is_scheduled;

   sched = job->schedule;
   if (!sched || !job->is_enabled() || (sched && !sched->is_enabled()) ||
       (job->client && !job->client->is_enabled())) {
      return NULL;                 /* no nothing to report */
   }

   /* Break down the time into components */
   now = time(NULL);
   endtime = now + (ndays * 60 * 60 * 24);

   if (run == NULL) {
      run = sched->run;
   } else {
      run = run->next;
   }
   for ( ; run; run=run->next) {
      /*
       * Find runs in next 24 hours.  Day 0 is today, so if
       *   ndays=1, look at today and tomorrow.
       */
      for (day = 0; day <= ndays; day++) {
         future = now + (day * 60 * 60 * 24);

         /* Break down the time into components */
         (void)localtime_r(&future, &tm);
         mday = tm.tm_mday - 1;
         wday = tm.tm_wday;
         month = tm.tm_mon;
         wom = mday / 7;
         woy = tm_woy(future);
         ldom = tm_ldom(month, tm.tm_year + 1900);

         is_scheduled = (bit_is_set(mday, run->mday) &&
                         bit_is_set(wday, run->wday) &&
                         bit_is_set(month, run->month) &&
                         bit_is_set(wom, run->wom) &&
                         bit_is_set(woy, run->woy)) ||
                        (bit_is_set(month, run->month) &&
                         bit_is_set(31, run->mday) && mday == ldom);

#ifdef xxx
         Pmsg2(000, "day=%d is_scheduled=%d\n", day, is_scheduled);
         Pmsg1(000, "bit_set_mday=%d\n", bit_is_set(mday, run->mday));
         Pmsg1(000, "bit_set_wday=%d\n", bit_is_set(wday, run->wday));
         Pmsg1(000, "bit_set_month=%d\n", bit_is_set(month, run->month));
         Pmsg1(000, "bit_set_wom=%d\n", bit_is_set(wom, run->wom));
         Pmsg1(000, "bit_set_woy=%d\n", bit_is_set(woy, run->woy));
#endif

         if (is_scheduled) { /* Jobs scheduled on that day */
#ifdef xxx
            char buf[300], num[10];
            bsnprintf(buf, sizeof(buf), "tm.hour=%d hour=", tm.tm_hour);
            for (i=0; i<24; i++) {
               if (bit_is_set(i, run->hour)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            Pmsg1(000, "%s", buf);
#endif
            /* find time (time_t) job is to be run */
            (void)localtime_r(&future, &runtm);
            for (i= 0; i < 24; i++) {
               if (bit_is_set(i, run->hour)) {
                  runtm.tm_hour = i;
                  runtm.tm_min = run->minute;
                  runtm.tm_sec = 0;
                  runtime = mktime(&runtm);
                  Dmsg2(200, "now=%d runtime=%lld\n", now, runtime);
                  if ((runtime > now) && (runtime < endtime)) {
                     Dmsg2(200, "Found it level=%d %c\n", run->level, run->level);
                     return run;         /* found it, return run resource */
                  }
               }
            }
         }
      }
   } /* end for loop over runs */
   /* Nothing found */
   return NULL;
}

/*
 * Fill in the remaining fields of the jcr as if it
 *  is going to run the job.
 */
bool complete_jcr_for_job(JCR *jcr, JOB *job, POOL *pool)
{
   POOL_DBR pr;

   bmemset(&pr, 0, sizeof(POOL_DBR));
   set_jcr_defaults(jcr, job);
   if (pool) {
      jcr->pool = pool;               /* override */
   }
   if (jcr->db) {
      Dmsg0(100, "complete_jcr close db\n");
      db_close_database(jcr, jcr->db);
      jcr->db = NULL;
   }

   Dmsg0(100, "complete_jcr open db\n");
   jcr->db = db_init_database(jcr, jcr->catalog->db_driver, jcr->catalog->db_name,
                jcr->catalog->db_user,
                jcr->catalog->db_password, jcr->catalog->db_address,
                jcr->catalog->db_port, jcr->catalog->db_socket,
                jcr->catalog->db_ssl_mode, jcr->catalog->db_ssl_key,
                jcr->catalog->db_ssl_cert, jcr->catalog->db_ssl_ca,
                jcr->catalog->db_ssl_capath, jcr->catalog->db_ssl_cipher,
                jcr->catalog->mult_db_connections,
                jcr->catalog->disable_batch_insert);
   if (!jcr->db || !db_open_database(jcr, jcr->db)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"),
                 jcr->catalog->db_name);
      if (jcr->db) {
         Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
         db_close_database(jcr, jcr->db);
         jcr->db = NULL;
      }
      return false;
   }
   bstrncpy(pr.Name, jcr->pool->name(), sizeof(pr.Name));
   while (!db_get_pool_record(jcr, jcr->db, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr, jcr->db, jcr->pool, POOL_OP_CREATE) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name,
            db_strerror(jcr->db));
         if (jcr->db) {
            db_close_database(jcr, jcr->db);
            jcr->db = NULL;
         }
         return false;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
      }
   }
   jcr->jr.PoolId = pr.PoolId;
   return true;
}


static void con_lock_release(void *arg)
{
   Vw(con_lock);
}

void do_messages(UAContext *ua, const char *cmd)
{
   char msg[2000];
   int mlen;
   bool do_truncate = false;

   if (ua->jcr) {
      dequeue_messages(ua->jcr);
   }
   Pw(con_lock);
   pthread_cleanup_push(con_lock_release, (void *)NULL);
   rewind(con_fd);
   while (fgets(msg, sizeof(msg), con_fd)) {
      mlen = strlen(msg);
      ua->UA_sock->msg = check_pool_memory_size(ua->UA_sock->msg, mlen+1);
      strcpy(ua->UA_sock->msg, msg);
      ua->UA_sock->msglen = mlen;
      ua->UA_sock->send();
      do_truncate = true;
   }
   if (do_truncate) {
      (void)ftruncate(fileno(con_fd), 0L);
   }
   console_msg_pending = FALSE;
   ua->user_notified_msg_pending = FALSE;
   pthread_cleanup_pop(0);
   Vw(con_lock);
}


int qmessagescmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending && ua->auto_display_messages) {
      do_messages(ua, cmd);
   }
   return 1;
}

int messagescmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   } else {
      ua->UA_sock->fsend(_("You have no messages.\n"));
   }
   return 1;
}

/*
 * Callback routine for "printing" database file listing
 */
void prtit(void *ctx, const char *msg)
{
   UAContext *ua = (UAContext *)ctx;

   if (ua) ua->send_msg("%s", msg);
}

/*
 * The following UA methods are mainly intended for GUI
 * programs
 */

/* 
 * This is an event message that will go to a log or the catalog
 */

void UAContext::send_events(const char *code, const char *type, const char *fmt, ...)
{
   POOL_MEM tmp(PM_MESSAGE);
   va_list arg_ptr;
   va_start(arg_ptr, fmt);
   bvsnprintf(tmp.c_str(), tmp.size(), fmt, arg_ptr);
   va_end(arg_ptr);

   events_send_msg(this->jcr, code, type, this->name, (intptr_t)this, "%s", tmp.c_str());
}

/*
 * This is a message that should be displayed on the user's
 *  console.
 */
void UAContext::send_msg(const char *fmt, ...)
{
   va_list arg_ptr;
   va_start(arg_ptr, fmt);
   bmsg(this, fmt, arg_ptr);
   va_end(arg_ptr);
}

/*
 * This is an error condition with a command. The gui should put
 *  up an error or critical dialog box.  The command is aborted.
 */
void UAContext::error_msg(const char *fmt, ...)
{
   BSOCK *bs = UA_sock;
   va_list arg_ptr;

   if (bs && api) bs->signal(BNET_ERROR_MSG);
   va_start(arg_ptr, fmt);
   bmsg(this, fmt, arg_ptr);
   va_end(arg_ptr);
}

/*
 * This is a warning message, that should bring up a warning
 *  dialog box on the GUI. The command is not aborted, but something
 *  went wrong.
 */
void UAContext::warning_msg(const char *fmt, ...)
{
   BSOCK *bs = UA_sock;
   va_list arg_ptr;

   if (bs && api) bs->signal(BNET_WARNING_MSG);
   va_start(arg_ptr, fmt);
   bmsg(this, fmt, arg_ptr);
   va_end(arg_ptr);
}

/*
 * This is an information message that should probably be put
 *  into the status line of a GUI program.
 */
void UAContext::info_msg(const char *fmt, ...)
{
   BSOCK *bs = UA_sock;
   va_list arg_ptr;

   if (bs && api) bs->signal(BNET_INFO_MSG);
   va_start(arg_ptr, fmt);
   bmsg(this, fmt, arg_ptr);
   va_end(arg_ptr);
}
