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
 *   Bacula Director -- fd_cmds.c -- send commands to File daemon
 *
 *     Kern Sibbald, October MM
 *
 *    This routine is run as a separate thread.  There may be more
 *    work to be done to make it totally reentrant!!!!
 *
 *  Utility functions for sending info to File Daemon.
 *   These functions are used by both backup and verify.
 *
 */

#include "bacula.h"
#include "dird.h"
#include "findlib/find.h"

const int dbglvl = 400;

/* Commands sent to File daemon */
static char filesetcmd[]  = "fileset%s%s\n"; /* set full fileset */
static char jobcmd[]      = "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s\n";
/* Note, mtime_only is not used here -- implemented as file option */
static char levelcmd[]    = "level = %s%s%s mtime_only=%d %s%s\n";
static char runscript[]   = "Run OnSuccess=%u OnFailure=%u AbortOnError=%u When=%u Command=%s\n";
static char runbeforenow[]= "RunBeforeNow\n";
static char bandwidthcmd[] = "setbandwidth=%lld Job=%s\n";
static char component_info[] = "component_info\n";

/* Responses received from File daemon */
static char OKinc[]          = "2000 OK include\n";
static char OKjob[]          = "2000 OK Job";
static char OKlevel[]        = "2000 OK level\n";
static char OKRunScript[]    = "2000 OK RunScript\n";
static char OKRunBeforeNow[] = "2000 OK RunBeforeNow\n";
static char OKRestoreObject[] = "2000 OK ObjectRestored\n";
static char OKComponentInfo[] = "2000 OK ComponentInfo\n";
static char OKBandwidth[]    = "2000 OK Bandwidth\n";

/* Forward referenced functions */
static bool send_list_item(JCR *jcr, const char *code, char *item, BSOCK *fd);

/* External functions */
extern DIRRES *director;
extern int FDConnectTimeout;

#define INC_LIST 0
#define EXC_LIST 1

static void delete_bsock_end_cb(JCR *jcr, void *ctx)
{
   BSOCK *socket = (BSOCK *)ctx;
   free_bsock(socket);
}

/*
 * Open connection with File daemon.
 * Try connecting every retry_interval (default 10 sec), and
 *   give up after max_retry_time (default 30 mins).
 * If the return code is 0, the error is stored inside jcr->errmsg
 * Need to call free_bsock() if an error occurs outside of a job
 */

int connect_to_file_daemon(JCR *jcr, int retry_interval, int max_retry_time,
                           int verbose)
{
   BSOCK   *fd = jcr->file_bsock;
   char ed1[30];
   utime_t heart_beat;
   int status;

   if (!jcr->client) {
      Mmsg(jcr->errmsg, _("[DE0017] File daemon not defined for current Job\n"));
      Dmsg0(10, "No Client defined for the job.\n");
      return 0;
   }

   if (jcr->client->heartbeat_interval) {
      heart_beat = jcr->client->heartbeat_interval;
   } else {
      heart_beat = director->heartbeat_interval;
   }
   if (!is_bsock_open(jcr->file_bsock)) {
      char name[MAX_NAME_LENGTH + 100];
      POOL_MEM buf, tmp;

      bstrncpy(name, _("Client: "), sizeof(name));
      bstrncat(name, jcr->client->name(), sizeof(name));

      if (jcr->client->allow_fd_connections) {
         Dmsg0(DT_NETWORK, "Try to use the existing socket if any\n");
         fd = jcr->client->getBSOCK(max_retry_time);
         /* Need to free the previous bsock, but without creating a race
          * condition. We will replace the BSOCK
          */
         if (fd && jcr->file_bsock) {
            Dmsg0(DT_NETWORK, "Found a socket, keep it!\n");
            job_end_push(jcr, delete_bsock_end_cb, (void *)jcr->file_bsock);
         }
         if (!fd) {
            Mmsg(jcr->errmsg, "[DE0010] No socket found of the client\n");
            return 0;
         }
         jcr->file_bsock = fd;
         fd->set_jcr(jcr);
         /* TODO: Need to set TLS down  */
         if (fd->tls) {
            fd->free_tls();
         }
      } else {

         if (!fd) {
            fd = jcr->file_bsock = new_bsock();
         }

         fd->set_source_address(director->DIRsrc_addr);
         if (!fd->connect(jcr,retry_interval,
                          max_retry_time,
                          heart_beat, name,
                          get_client_address(jcr, jcr->client, buf.addr()),
                          NULL,
                          jcr->client->FDport,
                          verbose)) {
            fd->close();
            pm_strcpy(jcr->errmsg, fd->errmsg);
            return 0;
         }
         Dmsg0(10, "Opened connection with File daemon\n");
      }
   }
   fd->res = (RES *)jcr->client;      /* save resource in BSOCK */
   jcr->setJobStatus(JS_Running);

   if (!authenticate_file_daemon(jcr, &status, &jcr->errmsg)) {
      Dmsg1(10, "Authentication error with FD. %s\n", jcr->errmsg);
      return 0;
   }

   /*
    * Now send JobId and authorization key
    */
   if (jcr->sd_auth_key == NULL) {
      jcr->sd_auth_key = bstrdup("dummy");
   }
   fd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, jcr->VolSessionId,
             jcr->VolSessionTime, jcr->sd_auth_key);
   if (!jcr->keep_sd_auth_key && strcmp(jcr->sd_auth_key, "dummy")) {
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   }
   Dmsg1(100, ">filed: %s", fd->msg);
   if (bget_dirmsg(jcr, fd, BSOCK_TYPE_FD) > 0) {
       Dmsg1(110, "<filed: %s", fd->msg);
       if (strncmp(fd->msg, OKjob, strlen(OKjob)) != 0) {
          Mmsg(jcr->errmsg, _("[DE0011] File daemon \"%s\" rejected Job command: %s\n"),
             jcr->client->hdr.name, fd->msg);
          return 0;

       } else if (jcr->db) {
          CLIENT_DBR cr;
          memset(&cr, 0, sizeof(cr));
          bstrncpy(cr.Name, jcr->client->hdr.name, sizeof(cr.Name));
          cr.AutoPrune = jcr->client->AutoPrune;
          cr.FileRetention = jcr->client->FileRetention;
          cr.JobRetention = jcr->client->JobRetention;

          /* information about plugins can be found after the Uname */
          char *pos = strchr(fd->msg+strlen(OKjob)+1, ';');
          if (pos) {
             *pos = 0;
             bstrncpy(cr.Plugins, pos+1, sizeof(cr.Plugins));
          }
          bstrncpy(cr.Uname, fd->msg+strlen(OKjob)+1, sizeof(cr.Uname));

          if (!db_update_client_record(jcr, jcr->db, &cr)) {
             Jmsg(jcr, M_WARNING, 0, _("[DE0008] Error updating Client record. ERR=%s\n"),
                  db_strerror(jcr->db));
          }
       }
   } else {
      Mmsg(jcr->errmsg, _("[DE0011] FD gave bad response to JobId command: %s\n"),
         fd->bstrerror());
      return 0;
   }
   return 1;
}

/*
 * This subroutine edits the last job start time into a
 *   "since=date/time" buffer that is returned in the
 *   variable since.  This is used for display purposes in
 *   the job report.  The time in jcr->stime is later
 *   passed to tell the File daemon what to do.
 */
void get_level_since_time(JCR *jcr, char *since, int since_len)
{
   int JobLevel;
   bool have_full;
   bool do_full = false;
   bool do_vfull = false;
   bool do_diff = false;
   bool print_reason2 = false;
   utime_t now;
   utime_t last_full_time = 0;
   utime_t last_diff_time;
   char prev_job[MAX_NAME_LENGTH], edl[50];
   char last_full_stime[MAX_TIME_LENGTH];
   const char *reason = "";
   POOL_MEM reason2;

   since[0] = 0;
   /* If job cloned and a since time already given, use it */
   if (jcr->cloned && jcr->stime && jcr->stime[0]) {
      bstrncpy(since, _(", since="), since_len);
      bstrncat(since, jcr->stime, since_len);
      return;
   }
   /* Make sure stime buffer is allocated */
   if (!jcr->stime) {
      jcr->stime = get_pool_memory(PM_MESSAGE);
   }
   jcr->PrevJob[0] = jcr->stime[0] = 0;
   last_full_stime[0] = 0;

   /*
    * Lookup the last FULL backup job to get the time/date for a
    * differential or incremental save.
    */
   switch (jcr->getJobLevel()) {
   case L_DIFFERENTIAL:
   case L_INCREMENTAL:
      POOLMEM *stime = get_pool_memory(PM_MESSAGE);
      /* Look up start time of last Full job */
      now = (utime_t)time(NULL);
      jcr->jr.JobId = 0;     /* flag to return since time */
      /*
       * This is probably redundant, but some of the code below
       * uses jcr->stime, so don't remove unless you are sure.
       */
      if (!db_find_job_start_time(jcr, jcr->db, &jcr->jr, &jcr->stime, jcr->PrevJob)) {
         do_full = true;
         reason = _("No prior or suitable Full backup found in catalog. ");
      }
      have_full = db_find_last_job_start_time(jcr, jcr->db, &jcr->jr,
                                              &stime, prev_job, L_FULL);
      if (have_full) {
         last_full_time = str_to_utime(stime);
         bstrncpy(last_full_stime, stime, sizeof(last_full_stime));

      } else {
         do_full = true;               /* No full, upgrade to one */

         /* We try to determine if we have a previous Full backup with an other FileSetId */
         DBId_t id = jcr->jr.FileSetId;
         jcr->jr.FileSetId = 0;
         if (db_find_last_job_start_time(jcr, jcr->db, &jcr->jr, &stime, prev_job, L_FULL)) {
            FILESET_DBR fs;
            memset(&fs, 0, sizeof(fs));
            fs.FileSetId = id;
            /* Print more information about the last fileset */
            if (db_get_fileset_record(jcr, jcr->db, &fs)) {
               Mmsg(reason2, _("The FileSet \"%s\" was modified on %s, this is after the last successful backup on %s."),
                    fs.FileSet, fs.cCreateTime, stime);
               print_reason2=true;
            }
            reason = _("No prior or suitable Full backup found in catalog for the current FileSet. ");
         } else {
            reason = _("No prior or suitable Full backup found in catalog. ");
         }
         jcr->jr.FileSetId = id;
      }
      Dmsg4(50, "have_full=%d do_full=%d now=%lld full_time=%lld\n", have_full,
            do_full, now, last_full_time);
      /* Make sure the last diff is recent enough */
      if (have_full && jcr->getJobLevel() == L_INCREMENTAL && jcr->job->MaxDiffInterval > 0) {
         /* Lookup last diff job */
         if (db_find_last_job_start_time(jcr, jcr->db, &jcr->jr,
                                         &stime, prev_job, L_DIFFERENTIAL)) {
            last_diff_time = str_to_utime(stime);
            /* If no Diff since Full, use Full time */
            if (last_diff_time < last_full_time) {
               last_diff_time = last_full_time;
            }
            Dmsg2(50, "last_diff_time=%lld last_full_time=%lld\n", last_diff_time,
                  last_full_time);
         } else {
            /* No last differential, so use last full time */
            last_diff_time = last_full_time;
            Dmsg1(50, "No last_diff_time setting to full_time=%lld\n", last_full_time);
         }
         do_diff = ((now - last_diff_time) >= jcr->job->MaxDiffInterval);
         if (do_diff) {
            reason = _("Max Diff Interval exceeded. ");
         }
         Dmsg2(50, "do_diff=%d diffInter=%lld\n", do_diff, jcr->job->MaxDiffInterval);
      }
      /* Note, do_full takes precedence over do_vfull and do_diff */
      if (have_full && jcr->job->MaxFullInterval > 0) {
         do_full = ((now - last_full_time) >= jcr->job->MaxFullInterval);
         if (do_full) {
            reason = _("Max Full Interval exceeded. ");
         }

      } else if (have_full && jcr->job->MaxVirtualFullInterval > 0) { /* Not used in BEE, not a real job */
         do_vfull = ((now - last_full_time) >= jcr->job->MaxVirtualFullInterval);
      }
      
      free_pool_memory(stime);

      if (do_full) {
         /* No recent Full job found, so upgrade this one to Full */
         if (print_reason2) {
            Jmsg(jcr, M_INFO, 0, "%s\n", reason2.c_str());
         }
         Jmsg(jcr, M_INFO, 0, _("%sDoing FULL backup.\n"), reason);
         bsnprintf(since, since_len, _(" (upgraded from %s)"),
                   level_to_str(edl , sizeof(edl), jcr->getJobLevel()));
         jcr->setJobLevel(jcr->jr.JobLevel = L_FULL);

      } else if (do_vfull) {
         /* No recent Full job found, and MaxVirtualFull is set so upgrade this one to Virtual Full */
         Jmsg(jcr, M_INFO, 0, "%s", db_strerror(jcr->db));
         Jmsg(jcr, M_INFO, 0, _("No prior or suitable Full backup found in catalog. Doing Virtual FULL backup.\n"));
         bsnprintf(since, since_len, _(" (upgraded from %s)"),
                   level_to_str(edl, sizeof(edl), jcr->getJobLevel()));
         jcr->setJobLevel(jcr->jr.JobLevel = L_VIRTUAL_FULL);

      } else if (do_diff) {
         /* No recent diff job found, so upgrade this one to Diff */
         Jmsg(jcr, M_INFO, 0, _("%sDoing Differential backup.\n"), reason);
         pm_strcpy(jcr->stime, last_full_stime);
         bsnprintf(since, since_len, _(", since=%s (upgraded from %s)"),
                   jcr->stime, level_to_str(edl, sizeof(edl), jcr->getJobLevel()));

         jcr->setJobLevel(jcr->jr.JobLevel = L_DIFFERENTIAL);

      } else {
         if (jcr->job->rerun_failed_levels) {

            POOLMEM *etime = get_pool_memory(PM_MESSAGE);

            /* Get the end time of our most recent successfull backup for this job */
            /* This will be used to see if there have been any failures since then */
            if (db_find_last_job_end_time(jcr, jcr->db, &jcr->jr, &etime, prev_job)) {

               /* See if there are any failed Differential/Full backups since the completion */
               /* of our last successful backup for this job                                 */
               if (db_find_failed_job_since(jcr, jcr->db, &jcr->jr,
                                         etime, JobLevel)) {
                 /* If our job is an Incremental and we have a failed job then upgrade.              */
                 /* If our job is a Differential and the failed job is a Full then upgrade.          */
                 /* Otherwise there is no reason to upgrade.                                         */
                 if ((jcr->getJobLevel() == L_INCREMENTAL) || 
                     ((jcr->getJobLevel() == L_DIFFERENTIAL) && (JobLevel == L_FULL))) {
                    Jmsg(jcr, M_INFO, 0, _("Prior failed job found in catalog. Upgrading to %s.\n"),
                         level_to_str(edl, sizeof(edl), JobLevel));
                    bsnprintf(since, since_len, _(" (upgraded from %s)"),
                              level_to_str(edl, sizeof(edl), jcr->getJobLevel()));
                    jcr->setJobLevel(jcr->jr.JobLevel = JobLevel);
                    jcr->jr.JobId = jcr->JobId;
                    break;
                 }
               }
            }
            free_pool_memory(etime);
         }
         bstrncpy(since, _(", since="), since_len);
         bstrncat(since, jcr->stime, since_len);
      }
      jcr->jr.JobId = jcr->JobId;
      break;
   }
   Dmsg3(100, "Level=%c last start time=%s job=%s\n",
         jcr->getJobLevel(), jcr->stime, jcr->PrevJob);
}

static void send_since_time(JCR *jcr)
{
   BSOCK   *fd = jcr->file_bsock;
   utime_t stime;
   char ed1[50];

   stime = str_to_utime(jcr->stime);
   fd->fsend(levelcmd, "", NT_("since_utime "), edit_uint64(stime, ed1), 0,
             NT_("prev_job="), jcr->PrevJob);
   while (bget_dirmsg(jcr, fd, BSOCK_TYPE_FD) >= 0) {  /* allow him to poll us to sync clocks */
      Jmsg(jcr, M_INFO, 0, "%s\n", fd->msg);
   }
}

bool send_bwlimit(JCR *jcr, const char *Job)
{
   BSOCK *fd = jcr->file_bsock;
   if (jcr->FDVersion >= 4) {
      fd->fsend(bandwidthcmd, jcr->max_bandwidth, Job);
      if (!response(jcr, fd, BSOCK_TYPE_FD, OKBandwidth, "Bandwidth", DISPLAY_ERROR)) {
         jcr->max_bandwidth = 0;      /* can't set bandwidth limit */
         return false;
      }
   }
   return true;
}

/*
 * Send level command to FD.
 * Used for backup jobs and estimate command.
 */
bool send_level_command(JCR *jcr)
{
   BSOCK   *fd = jcr->file_bsock;
   const char *accurate = jcr->accurate?"accurate_":"";
   const char *not_accurate = "";
   const char *rerunning = jcr->rerunning?" rerunning ":" ";
   /*
    * Send Level command to File daemon
    */
   switch (jcr->getJobLevel()) {
   case L_BASE:
      fd->fsend(levelcmd, not_accurate, "base", rerunning, 0, "", "");
      break;
   /* L_NONE is the console, sending something off to the FD */
   case L_NONE:
   case L_FULL:
      fd->fsend(levelcmd, not_accurate, "full", rerunning, 0, "", "");
      break;
   case L_DIFFERENTIAL:
      fd->fsend(levelcmd, accurate, "differential", rerunning, 0, "", "");
      send_since_time(jcr);
      break;
   case L_INCREMENTAL:
      fd->fsend(levelcmd, accurate, "incremental", rerunning, 0, "", "");
      send_since_time(jcr);
      break;
   case L_SINCE:
   default:
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented backup level %d %c\n"),
         jcr->getJobLevel(), jcr->getJobLevel());
      return 0;
   }
   Dmsg1(120, ">filed: %s", fd->msg);
   if (!response(jcr, fd, BSOCK_TYPE_FD, OKlevel, "Level", DISPLAY_ERROR)) {
      return false;
   }
   return true;
}

/*
 * Send either an Included or an Excluded list to FD
 */
static bool send_fileset(JCR *jcr)
{
   FILESET *fileset = jcr->fileset;
   BSOCK   *fd = jcr->file_bsock;
   STORE   *store = jcr->store_mngr->get_wstore();
   int num;
   bool include = true;

   for ( ;; ) {
      if (include) {
         num = fileset->num_includes;
      } else {
         num = fileset->num_excludes;
      }
      for (int i=0; i<num; i++) {
         char *item;
         INCEXE *ie;
         int j, k;

         if (include) {
            ie = fileset->include_items[i];
            fd->fsend("I\n");
         } else {
            ie = fileset->exclude_items[i];
            fd->fsend("E\n");
         }
         if (ie->ignoredir) {
            fd->fsend("Z %s\n", ie->ignoredir);
         }
         for (j=0; j<ie->num_opts; j++) {
            FOPTS *fo = ie->opts_list[j];
            bool enhanced_wild = false;
            bool stripped_opts = false;
            bool compress_disabled = false;
            char newopts[MAX_FOPTS];

            for (k=0; fo->opts[k]!='\0'; k++) {
               if (fo->opts[k]=='W') {
                  enhanced_wild = true;
                  break;
               }
            }

            /*
             * Strip out compression option Zn if disallowed
             *  for this Storage.
             * Strip out dedup option dn if old FD
             */
            bool strip_compress = store && !store->AllowCompress;
            if (strip_compress || jcr->FDVersion >= 11) {
               int j = 0;
               for (k=0; fo->opts[k]!='\0'; k++) {
                  /* Z compress option is followed by the single-digit compress level or 'o' */
                  if (strip_compress && fo->opts[k]=='Z') {
                     stripped_opts = true;
                     compress_disabled = true;
                     k++;                /* skip level */
                  } else if (jcr->FDVersion < 11 && fo->opts[k]=='d') {
                     stripped_opts = true;
                     k++;              /* skip level */
                  } else {
                     newopts[j] = fo->opts[k];
                     j++;
                  }
               }
               newopts[j] = '\0';
               if (compress_disabled) {
                  Jmsg(jcr, M_INFO, 0,
                      _("FD compression disabled for this Job because AllowCompression=No in Storage resource.\n") );
               }
            }
            if (stripped_opts) {
               /* Send the new trimmed option set without overwriting fo->opts */
               fd->fsend("O %s\n", newopts);
            } else {
               /* Send the original options */
               fd->fsend("O %s\n", fo->opts);
            }
            for (k=0; k<fo->regex.size(); k++) {
               fd->fsend("R %s\n", fo->regex.get(k));
            }
            for (k=0; k<fo->regexdir.size(); k++) {
               fd->fsend("RD %s\n", fo->regexdir.get(k));
            }
            for (k=0; k<fo->regexfile.size(); k++) {
               fd->fsend("RF %s\n", fo->regexfile.get(k));
            }
            for (k=0; k<fo->wild.size(); k++) {
               fd->fsend("W %s\n", fo->wild.get(k));
            }
            for (k=0; k<fo->wilddir.size(); k++) {
               fd->fsend("WD %s\n", fo->wilddir.get(k));
            }
            for (k=0; k<fo->wildfile.size(); k++) {
               fd->fsend("WF %s\n", fo->wildfile.get(k));
            }
            for (k=0; k<fo->wildbase.size(); k++) {
               fd->fsend("W%c %s\n", enhanced_wild ? 'B' : 'F', fo->wildbase.get(k));
            }
            for (k=0; k<fo->base.size(); k++) {
               fd->fsend("B %s\n", fo->base.get(k));
            }
            for (k=0; k<fo->fstype.size(); k++) {
               fd->fsend("X %s\n", fo->fstype.get(k));
            }
            for (k=0; k<fo->drivetype.size(); k++) {
               fd->fsend("XD %s\n", fo->drivetype.get(k));
            }
            if (fo->plugin) {
               fd->fsend("G %s\n", fo->plugin);
            }
            if (fo->reader) {
               fd->fsend("D %s\n", fo->reader);
            }
            if (fo->writer) {
               fd->fsend("T %s\n", fo->writer);
            }
            fd->fsend("N\n");
         }

         for (j=0; j<ie->name_list.size(); j++) {
            item = (char *)ie->name_list.get(j);
            if (!send_list_item(jcr, "F ", item, fd)) {
               goto bail_out;
            }
         }
         fd->fsend("N\n");
         for (j=0; j<ie->plugin_list.size(); j++) {
            item = (char *)ie->plugin_list.get(j);
            if (!send_list_item(jcr, "P ", item, fd)) {
               goto bail_out;
            }
         }
         fd->fsend("N\n");
      }
      if (!include) {                 /* If we just did excludes */
         break;                       /*   all done */
      }
      include = false;                /* Now do excludes */
   }

   fd->signal(BNET_EOD);              /* end of data */
   if (!response(jcr, fd, BSOCK_TYPE_FD, OKinc, "Include", DISPLAY_ERROR)) {
      goto bail_out;
   }
   return true;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);
   return false;

}

static bool send_list_item(JCR *jcr, const char *code, char *item, BSOCK *fd)
{
   BPIPE *bpipe;
   FILE *ffd;
   char buf[2000];
   int optlen, stat;
   char *p = item;

   switch (*p) {
   case '|':
      p++;                      /* skip over the | */
      fd->msg = edit_job_codes(jcr, fd->msg, p, "");
      bpipe = open_bpipe(fd->msg, 0, "r");
      if (!bpipe) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"),
            p, be.bstrerror());
         return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf+optlen, sizeof(buf)-optlen, bpipe->rfd)) {
         fd->msglen = Mmsg(fd->msg, "%s", buf);
         Dmsg2(500, "Inc/exc len=%d: %s", fd->msglen, fd->msg);
         if (!fd->send()) {
            close_bpipe(bpipe);
            Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
            return false;
         }
      }
      if ((stat=close_bpipe(bpipe)) != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. ERR=%s\n"),
            p, be.bstrerror(stat));
         return false;
      }
      break;
   case '<':
      p++;                      /* skip over < */
      if ((ffd = bfopen(p, "rb")) == NULL) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot open included file: %s. ERR=%s\n"),
            p, be.bstrerror());
         return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf+optlen, sizeof(buf)-optlen, ffd)) {
         fd->msglen = Mmsg(fd->msg, "%s", buf);
         if (!fd->send()) {
            fclose(ffd);
            Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
            return false;
         }
      }
      fclose(ffd);
      break;
   case '\\':
      p++;                      /* skip over \ */
      /* Note, fall through wanted */
   default:
      pm_strcpy(fd->msg, code);
      fd->msglen = pm_strcat(fd->msg, p);
      Dmsg1(500, "Inc/Exc name=%s\n", fd->msg);
      if (!fd->send()) {
         Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
         return false;
      }
      break;
   }
   return true;
}


/*
 * Send include list to File daemon
 */
bool send_include_list(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   if (jcr->fileset->new_include) {
      fd->fsend(filesetcmd,
                jcr->fileset->enable_vss ? " vss=1" : "",
                jcr->fileset->enable_snapshot ? " snap=1" : "");
      return send_fileset(jcr);
   }
   return true;
}

/*
 * Send an include list with a plugin and listing=<path> parameter
 */
bool send_ls_plugin_fileset(JCR *jcr, const char *plugin, const char *path)
{
   BSOCK *fd = jcr->file_bsock;
   fd->fsend(filesetcmd, "" /* no vss */, "" /* no snapshot */);

   fd->fsend("I\n");
   fd->fsend("O h\n");         /* is it required? */
   fd->fsend("N\n");
   fd->fsend("P %s%s listing=%s\n", plugin, strchr(plugin, ':') == NULL ? ":" : "", path);
   fd->fsend("N\n");
   fd->signal(BNET_EOD);              /* end of data */

   if (!response(jcr, fd, BSOCK_TYPE_FD, OKinc, "Include", DISPLAY_ERROR)) {
      return false;
   }
   return true;
}

/*
 * Send a include list with only one directory and recurse=no
 */
bool send_ls_fileset(JCR *jcr, const char *path)
{
   BSOCK *fd = jcr->file_bsock;
   fd->fsend(filesetcmd, "" /* no vss */, "" /* no snapshot */);

   fd->fsend("I\n");
   fd->fsend("O h\n");         /* Limit recursion to one directory */
   fd->fsend("N\n");
   fd->fsend("F %s\n", path);
   fd->fsend("N\n");
   fd->signal(BNET_EOD);              /* end of data */

   if (!response(jcr, fd, BSOCK_TYPE_FD, OKinc, "Include", DISPLAY_ERROR)) {
      return false;
   }
   return true;
}


/*
 * Send exclude list to File daemon
 *   Under the new scheme, the Exclude list
 *   is part of the FileSet sent with the
 *   "include_list" above.
 */
bool send_exclude_list(JCR *jcr)
{
   return true;
}

/* TODO: drop this with runscript.old_proto in bacula 1.42 */
static char runbefore[]   = "RunBeforeJob %s\n";
static char runafter[]    = "RunAfterJob %s\n";
static char OKRunBefore[] = "2000 OK RunBefore\n";
static char OKRunAfter[]  = "2000 OK RunAfter\n";

int send_runscript_with_old_proto(JCR *jcr, int when, POOLMEM *msg)
{
   int ret;
   Dmsg1(120, "bdird: sending old runcommand to fd '%s'\n",msg);
   if (when & SCRIPT_Before) {
      jcr->file_bsock->fsend(runbefore, msg);
      ret = response(jcr, jcr->file_bsock, BSOCK_TYPE_FD, OKRunBefore, "ClientRunBeforeJob", DISPLAY_ERROR);
   } else {
      jcr->file_bsock->fsend(runafter, msg);
      ret = response(jcr, jcr->file_bsock, BSOCK_TYPE_FD, OKRunAfter, "ClientRunAfterJob", DISPLAY_ERROR);
   }
   return ret;
} /* END OF TODO */

/*
 * Send RunScripts to File daemon
 * 1) We send all runscript to FD, they can be executed Before, After, or twice
 * 2) Then, we send a "RunBeforeNow" command to the FD to tell him to do the
 *    first run_script() call. (ie ClientRunBeforeJob)
 */
int send_runscripts_commands(JCR *jcr)
{
   POOLMEM *msg = get_pool_memory(PM_FNAME);
   BSOCK *fd = jcr->file_bsock;
   RUNSCRIPT *cmd;
   bool launch_before_cmd = false;
   POOLMEM *ehost = get_pool_memory(PM_FNAME);
   int result;

   Dmsg0(120, "bdird: sending runscripts to fd\n");
   if (!jcr->job->RunScripts) {
      goto norunscript;
   }
   foreach_alist(cmd, jcr->job->RunScripts) {
      if (cmd->can_run_at_level(jcr->getJobLevel()) && cmd->target) {
         ehost = edit_job_codes(jcr, ehost, cmd->target, "");
         Dmsg2(200, "bdird: runscript %s -> %s\n", cmd->target, ehost);

         if (strcmp(ehost, jcr->client->name()) == 0) {
            pm_strcpy(msg, cmd->command);
            bash_spaces(msg);

            Dmsg1(120, "bdird: sending runscripts to fd '%s'\n", cmd->command);

            /* TODO: remove this with bacula 1.42 */
            if (cmd->old_proto) {
               result = send_runscript_with_old_proto(jcr, cmd->when, msg);

            } else {
               /* On the FileDaemon, AtJobCompletion is a synonym of After */
               cmd->when = (cmd->when == SCRIPT_AtJobCompletion) ? SCRIPT_After : cmd->when;
               fd->fsend(runscript, cmd->on_success,
                                    cmd->on_failure,
                                    cmd->fail_on_error,
                                    cmd->when,
                                    msg);

               result = response(jcr, fd, BSOCK_TYPE_FD, OKRunScript, "RunScript", DISPLAY_ERROR);
               launch_before_cmd = true;
            }

            if (!result) {
               goto bail_out;
            }
         }
         /* TODO : we have to play with other client */
         /*
           else {
           send command to an other client
           }
         */
      }
   }

   /* Tell the FD to execute the ClientRunBeforeJob */
   if (launch_before_cmd) {
      fd->fsend(runbeforenow);
      if (!response(jcr, fd, BSOCK_TYPE_FD, OKRunBeforeNow, "RunBeforeNow", DISPLAY_ERROR)) {
        goto bail_out;
      }
   }
norunscript:
   free_pool_memory(msg);
   free_pool_memory(ehost);
   return 1;

bail_out:
   Jmsg(jcr, M_FATAL, 0, _("Client \"%s\" RunScript failed.\n"), ehost);
   free_pool_memory(msg);
   free_pool_memory(ehost);
   return 0;
}

struct OBJ_CTX {
   JCR *jcr;
   int count;
};

static int restore_object_handler(void *ctx, int num_fields, char **row)
{
   OBJ_CTX *octx = (OBJ_CTX *)ctx;
   JCR *jcr = octx->jcr;
   BSOCK *fd;

   fd = jcr->file_bsock;
   if (jcr->is_job_canceled()) {
      return 1;
   }
   /* Old File Daemon doesn't handle restore objects */
   if (jcr->FDVersion < 3) {
      Jmsg(jcr, M_WARNING, 0, _("Client \"%s\" may not be used to restore "
                                "this job. Please upgrade your client.\n"),
           jcr->client->name());
      return 1;
   }

   if (jcr->FDVersion < 5) {    /* Old version without PluginName */
      fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s\n",
                row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
   } else {
      /* bash spaces from PluginName */
      bash_spaces(row[9]);
      fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s,%s\n",
                row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[9]);
   }
   Dmsg1(010, "Send obj hdr=%s", fd->msg);

   fd->msglen = pm_strcpy(fd->msg, row[7]);
   fd->send();                            /* send Object name */

   Dmsg1(010, "Send obj: %s\n", fd->msg);

//   fd->msglen = str_to_uint64(row[1]);   /* object length */
//   Dmsg1(000, "obj size: %lld\n", (uint64_t)fd->msglen);

   /* object */
   db_unescape_object(jcr, jcr->db,
                      row[8],                /* Object  */
                      str_to_uint64(row[1]), /* Object length */
                      &fd->msg, &fd->msglen);
   fd->send();                           /* send object */
   octx->count++;

   if (debug_level > 100) {
      for (int i=0; i < fd->msglen; i++)
         if (!fd->msg[i])
            fd->msg[i] = ' ';
      Dmsg1(000, "Send obj: %s\n", fd->msg);
   }

   return 0;
}

/* Send the restore file list to the plugin */
void feature_send_restorefilelist(JCR *jcr, const char *plugin)
{
   if (!jcr->bsr_list) {
      Dmsg0(10, "Unable to send restore file list\n");
      return;
   }
   jcr->file_bsock->fsend("restorefilelist plugin=%s\n", plugin);
   scan_bsr(jcr);
   jcr->file_bsock->signal(BNET_EOD);
}

typedef struct {
   const char *name;         /* Name of the feature */
   alist      *plugins;      /* List of the plugins that can use the feature */
   void (*handler)(JCR *, const char *plugin); /* handler for the feature */
} PluginFeatures;

/* List of all the supported features. This table is global and 
 * each job should do a copy.
 */
const static PluginFeatures plugin_features[] = {
   /* name                             plugins  handler */
   {PLUGIN_FEATURE_RESTORELISTFILES,   NULL,    feature_send_restorefilelist},
   {NULL,                              NULL,    NULL}
};

#define NB_FEATURES (sizeof(plugin_features) / sizeof(PluginFeatures))

/* Take a plugin and a feature name, and fill the Feature list */
static void fill_feature_list(JCR *jcr, PluginFeatures *features, char *plugin, char *name)
{
   for (int i=0; features[i].name != NULL ; i++) {
      if (strcasecmp(features[i].name, name) == 0) {
         if (features[i].plugins == NULL) {
            features[i].plugins = New(alist(10, owned_by_alist));
         }
         Dmsg2(10, "plugin=%s match feature %s\n", plugin, name);
         features[i].plugins->append(bstrdup(plugin));
         break;
      }
   }
}

/* Free the memory allocated by get_plugin_features() */
static void free_plugin_features(PluginFeatures *features)
{
   if (!features) {
      return;
   }
   for (int i=0; features[i].name != NULL ; i++) {
      if (features[i].plugins != NULL) {
         delete features[i].plugins;
      }
   }
   free(features);
}

/* Can be used in Backup or Restore jobs to know what a plugin can do
 * Need to call free_plugin_features() to release the PluginFeatures argument
 */
static bool get_plugin_features(JCR *jcr, PluginFeatures **ret)
{
   POOL_MEM buf;
   char ed1[128];
   BSOCK *fd = jcr->file_bsock;
   PluginFeatures *features;
   char *p, *start;

   *ret = NULL;

   if (jcr->FDVersion < 14 || jcr->FDVersion == 213 || jcr->FDVersion == 214) {
      return false;              /* File Daemon too old or not compatible */
   }

   /* Copy the features table */
   features = (PluginFeatures *)malloc(sizeof(PluginFeatures) * NB_FEATURES);
   memcpy(features, plugin_features, sizeof(plugin_features));

   /* Get the list of the features that the plugins can request
    * ex: plugin=ndmp features=files,feature1,feature2
    */
   fd->fsend("PluginFeatures\n");

   while (bget_dirmsg(jcr, fd, BSOCK_TYPE_FD) > 0) {
      buf.check_size(fd->msglen+1);
      if (sscanf(fd->msg, "2000 plugin=%127s features=%s", ed1, buf.c_str()) == 2) {
         /* We have buf=feature1,feature2,feature3 */
         start = buf.c_str();
         while ((p = next_name(&start)) != NULL) {
            fill_feature_list(jcr, features, ed1, p);
         }
      } else {
         Dmsg1(10, "Something wrong with the protocol %s\n", fd->msg);
         free_plugin_features(features);
         return false;
      }
   }
   *ret = features;
   return true;
}

/* See with the FD if we need to send the list of all the files
 * to be restored before the start of the job
 */
bool send_restore_file_list(JCR *jcr)
{
   PluginFeatures *features=NULL;

   /* TODO: If we have more features, we can store the features list in the JCR */
   if (!get_plugin_features(jcr, &features)) {
      return true;              /* Not handled by FD */
   }

   /* Now, we can deal with what the plugins want */
   for (int i=0; features[i].name != NULL ; i++) {
      if (strcmp(features[i].name, PLUGIN_FEATURE_RESTORELISTFILES) == 0) {
         if (features[i].plugins != NULL) {
            char *plug;
            foreach_alist(plug, features[i].plugins) {
               features[i].handler(jcr, plug);
            }
         }
         break;
      }
   }

   free_plugin_features(features);
   return true;
}

/*
 * Send the plugin Restore Objects, which allow the
 *  plugin to get information early in the restore
 *  process.  The RestoreObjects were created during
 *  the backup by the plugin.
 */
bool send_restore_objects(JCR *jcr)
{
   char ed1[50];
   POOL_MEM query(PM_MESSAGE);
   BSOCK *fd;
   OBJ_CTX octx;

   if (!jcr->JobIds || !jcr->JobIds[0]) {
      return true;
   }
   octx.jcr = jcr;
   octx.count = 0;

   /* restore_object_handler is called for each file found */

   /* send restore objects for all jobs involved  */
   Mmsg(query, get_restore_objects, jcr->JobIds, FT_RESTORE_FIRST);
   db_sql_query(jcr->db, query.c_str(), restore_object_handler, (void *)&octx);

   /* send config objects for the current restore job */
   Mmsg(query, get_restore_objects,
        edit_uint64(jcr->JobId, ed1), FT_PLUGIN_CONFIG_FILLED);
   db_sql_query(jcr->db, query.c_str(), restore_object_handler, (void *)&octx);

   /*
    * Send to FD only if we have at least one restore object.
    * This permits backward compatibility with older FDs.
    */
   if (octx.count > 0) {
      fd = jcr->file_bsock;
      fd->fsend("restoreobject end\n");
      if (!response(jcr, fd, BSOCK_TYPE_FD, OKRestoreObject, "RestoreObject", DISPLAY_ERROR)) {
         Jmsg(jcr, M_FATAL, 0, _("RestoreObject failed.\n"));
         return false;
      }
   }
   return true;
}

/*
 * Send the plugin a list of component info files.  These
 *  were files that were created during the backup for
 *  the VSS plugin.  The list is a list of those component
 *  files that have been chosen for restore.  We
 *  send them before the Restore Objects.
 */
bool send_component_info(JCR *jcr)
{
   BSOCK *fd;
   char buf[2000];
   bool ok = true;

   if (!jcr->component_fd) {
      return true;           /* nothing to send */
   }
   /* Don't send if old version FD */
   if (jcr->FDVersion < 6) {
      goto bail_out;
   }

   rewind(jcr->component_fd);
   fd = jcr->file_bsock;
   fd->fsend(component_info);
   while (fgets(buf, sizeof(buf), jcr->component_fd)) {
      fd->fsend("%s", buf);
      Dmsg1(050, "Send component_info to FD: %s\n", buf);
   }
   fd->signal(BNET_EOD);
   if (!response(jcr, fd, BSOCK_TYPE_FD, OKComponentInfo, "ComponentInfo", DISPLAY_ERROR)) {
      Jmsg(jcr, M_FATAL, 0, _("ComponentInfo failed.\n"));
      ok = false;
   }

bail_out:
   fclose(jcr->component_fd);
   jcr->component_fd = NULL;
   unlink(jcr->component_fname);
   free_and_null_pool_memory(jcr->component_fname);
   return ok;
}

/*
 * Read the attributes from the File daemon for
 * a Verify job and store them in the catalog.
 */
int get_attributes_and_put_in_catalog(JCR *jcr)
{
   BSOCK   *fd;
   int n = 0;
   ATTR_DBR *ar = NULL;
   char digest[2*(MAXSTRING+1)+1];  /* escaped version of Digest */

   fd = jcr->file_bsock;
   jcr->jr.FirstIndex = 1;
   jcr->FileIndex = 0;
   /* Start transaction allocates jcr->attr and jcr->ar if needed */
   db_start_transaction(jcr, jcr->db);     /* start transaction if not already open */
   ar = jcr->ar;

   Dmsg0(120, "bdird: waiting to receive file attributes\n");
   /* Pickup file attributes and digest */
   while (!fd->errors && (n = bget_dirmsg(jcr, fd, BSOCK_TYPE_FD)) > 0) {
      int32_t file_index;
      int stream, len;
      char *p, *fn;
      char Digest[MAXSTRING+1];      /* either Verify opts or MD5/SHA1 digest */

      /* Stop here if canceled */
      if (jcr->is_job_canceled()) {
         jcr->cached_attribute = false;
         return 0;
      }

      if ((len = sscanf(fd->msg, "%ld %d %500s", &file_index, &stream, Digest)) != 3) { /* MAXSTRING */
         Jmsg(jcr, M_FATAL, 0, _("<filed: bad attributes, expected 3 fields got %d\n"
"msglen=%d msg=%s\n"), len, fd->msglen, fd->msg);
         jcr->setJobStatus(JS_ErrorTerminated);
         jcr->cached_attribute = false;
         return 0;
      }
      p = fd->msg;
      /* The following three fields were sscanf'ed above so skip them */
      skip_nonspaces(&p);             /* skip FileIndex */
      skip_spaces(&p);
      skip_nonspaces(&p);             /* skip Stream */
      skip_spaces(&p);
      skip_nonspaces(&p);             /* skip Opts_Digest */
      p++;                            /* skip space */
      Dmsg1(dbglvl, "Stream=%d\n", stream);
      if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_UNIX_ATTRIBUTES_EX) {
         if (jcr->cached_attribute) {
            Dmsg3(dbglvl, "Cached attr. Stream=%d fname=%s\n", ar->Stream, ar->fname,
               ar->attr);
            if (!db_create_file_attributes_record(jcr, jcr->db, ar)) {
               Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
            }
            jcr->cached_attribute = false;
         }
         /* Any cached attr is flushed so we can reuse jcr->attr and jcr->ar */
         fn = jcr->fname = check_pool_memory_size(jcr->fname, fd->msglen);
         while (*p != 0) {
            *fn++ = *p++;                /* copy filename */
         }
         *fn = *p++;                     /* term filename and point p to attribs */
         pm_strcpy(jcr->attr, p);        /* save attributes */
         jcr->JobFiles++;
         jcr->FileIndex = file_index;
         ar->attr = jcr->attr;
         ar->fname = jcr->fname;
         ar->FileIndex = file_index;
         ar->Stream = stream;
         ar->link = NULL;
         ar->JobId = jcr->JobId;
         ar->ClientId = jcr->ClientId;
         ar->PathId = 0;
         ar->Filename = NULL;
         ar->Digest = NULL;
         ar->DigestType = CRYPTO_DIGEST_NONE;
         ar->DeltaSeq = 0;
         jcr->cached_attribute = true;

         Dmsg2(dbglvl, "dird<filed: stream=%d %s\n", stream, jcr->fname);
         Dmsg1(dbglvl, "dird<filed: attr=%s\n", ar->attr);
         jcr->FileId = ar->FileId;
      /*
       * First, get STREAM_UNIX_ATTRIBUTES and fill ATTR_DBR structure
       * Next, we CAN have a CRYPTO_DIGEST, so we fill ATTR_DBR with it (or not)
       * When we get a new STREAM_UNIX_ATTRIBUTES, we known that we can add file to the catalog
       * At the end, we have to add the last file
       */
      } else if (crypto_digest_stream_type(stream) != CRYPTO_DIGEST_NONE) {
         if (jcr->FileIndex != file_index) {
            Jmsg3(jcr, M_ERROR, 0, _("%s index %d not same as attributes %d\n"),
               stream_to_ascii(stream), file_index, jcr->FileIndex);
            continue;
         }
         ar->Digest = digest;
         ar->DigestType = crypto_digest_stream_type(stream);
         db_escape_string(jcr, jcr->db, digest, Digest, strlen(Digest));
         Dmsg4(dbglvl, "stream=%d DigestLen=%d Digest=%s type=%d\n", stream,
               strlen(digest), digest, ar->DigestType);
      }
      jcr->jr.JobFiles = jcr->JobFiles = file_index;
      jcr->jr.LastIndex = file_index;
   }
   if (fd->is_error()) {
      Jmsg1(jcr, M_FATAL, 0, _("<filed: Network error getting attributes. ERR=%s\n"),
            fd->bstrerror());
      jcr->cached_attribute = false;
      return 0;
   }
   if (jcr->cached_attribute) {
      Dmsg3(dbglvl, "Cached attr with digest. Stream=%d fname=%s attr=%s\n", ar->Stream,
         ar->fname, ar->attr);
      if (!db_create_file_attributes_record(jcr, jcr->db, ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
      }
      jcr->cached_attribute = false;
   }
   jcr->setJobStatus(JS_Terminated);
   return 1;
}
