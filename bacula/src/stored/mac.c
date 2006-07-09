/*
 * SD -- mac.c --  responsible for doing
 *     migration, archive, and copy jobs.
 *
 *     Kern Sibbald, January MMVI
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "stored.h"

/* Import functions */
extern char Job_end[];   

/* Forward referenced subroutines */
static bool record_cb(DCR *dcr, DEV_RECORD *rec);


/*
 *  Read Data and send to File Daemon
 *   Returns: false on failure
 *            true  on success
 */
bool do_mac(JCR *jcr)
{
   bool ok = true;
   BSOCK *dir = jcr->dir_bsock;
   const char *Type;
   char ec1[50];
   DEVICE *dev;

   switch(jcr->JobType) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   default:
      Type = "Unknown";
      break;
   }


   Dmsg0(20, "Start read data.\n");

   if (!jcr->read_dcr || !jcr->dcr) {
      Jmsg(jcr, M_FATAL, 0, _("Read and write devices not properly initialized.\n"));
      goto bail_out;
   }
   Dmsg2(100, "read_dcr=%p write_dcr=%p\n", jcr->read_dcr, jcr->dcr);


   create_restore_volume_list(jcr);
   if (jcr->NumVolumes == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Volume names found for %s.\n"), Type);
      goto bail_out;
   }

   Dmsg3(200, "Found %d volumes names for %s. First=%s\n", jcr->NumVolumes,
      jcr->VolList->VolumeName, Type);

   /* Ready devices for reading and writing */
   if (!acquire_device_for_read(jcr->read_dcr) ||
       !acquire_device_for_append(jcr->dcr)) {
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      goto bail_out;
   }

   Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->dcr->dev->file, jcr->dcr->dev->block_num);
     

   set_jcr_job_status(jcr, JS_Running);
   dir_send_job_status(jcr);

   jcr->dcr->VolFirstIndex = jcr->dcr->VolLastIndex = 0;
   jcr->run_time = time(NULL);

   ok = read_records(jcr->read_dcr, record_cb, mount_next_read_volume);
   goto ok_out;

bail_out:
   ok = false;

ok_out:
   if (jcr->dcr) {
      dev = jcr->dcr->dev;
      if (ok || dev->can_write()) {
         /* Flush out final partial block of this session */
         if (!write_block_to_device(jcr->dcr)) {
            Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
                  dev->print_name(), dev->bstrerror());
            Dmsg0(100, _("Set ok=FALSE after write_block_to_device.\n"));
            ok = false;
         }
         Dmsg2(200, "Flush block to device pos %u:%u\n", dev->file, dev->block_num);
      }  


      if (ok && dev->is_dvd()) {
         ok = dvd_close_job(jcr->dcr);   /* do DVD cleanup if any */
      }
      /* Release the device -- and send final Vol info to DIR */
      release_device(jcr->dcr);
   }

   if (jcr->read_dcr) {
      if (!release_device(jcr->read_dcr)) {
         ok = false;
      }
   }

   free_restore_volume_list(jcr);


   if (!ok || job_canceled(jcr)) {
      discard_attribute_spool(jcr);
   } else {
      commit_attribute_spool(jcr);
   }

   dir_send_job_status(jcr);          /* update director */


   Dmsg0(30, "Done reading.\n");
   jcr->end_time = time(NULL);
   dequeue_messages(jcr);             /* send any queued messages */
   if (ok) {
      set_jcr_job_status(jcr, JS_Terminated);
   }
   generate_daemon_event(jcr, "JobEnd");
   bnet_fsend(dir, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1));
   Dmsg4(200, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles, ec1); 
       
   bnet_sig(dir, BNET_EOD);           /* send EOD to Director daemon */

   return ok;
}

/*
 * Called here for each record from read_records()
 *  Returns: true if OK
 *           false if error
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = jcr->dcr->dev;
   char buf1[100], buf2[100];
   int32_t stream;   
   
   /* If label and not for us, discard it */
   if (rec->FileIndex < 0 && rec->match_stat <= 0) {
      return true;
   }
   /* We want to write SOS_LABEL and EOS_LABEL discard all others */
   switch (rec->FileIndex) {                        
   case PRE_LABEL:
   case VOL_LABEL:
   case EOT_LABEL:
   case EOM_LABEL:
      return true;                    /* don't write vol labels */
   }
   /*
    * Modify record SessionId and SessionTime to correspond to
    * output.
    */
   rec->VolSessionId = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;
   Dmsg5(200, "before write_rec JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf1, rec->Stream,rec->FileIndex), rec->data_len);

   while (!write_record_to_block(jcr->dcr->block, rec)) {
      Dmsg4(200, "!write_record_to_block blkpos=%u:%u len=%d rem=%d\n", 
            dev->file, dev->block_num, rec->data_len, rec->remainder);
      if (!write_block_to_device(jcr->dcr)) {
         Dmsg2(90, "Got write_block_to_dev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
         Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
               dev->print_name(), dev->bstrerror());
         return false;
      }
      Dmsg2(200, "===== Wrote block new pos %u:%u\n", dev->file, dev->block_num);
   }
   jcr->JobBytes += rec->data_len;   /* increment bytes this job */
   if (rec->FileIndex > 0) {
      jcr->JobFiles = rec->FileIndex;
   } else {
      return true;                    /* don't send LABELs to Dir */
   }
   Dmsg5(500, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

   /* Send attributes and digest to Director for Catalog */
   stream = rec->Stream;
   if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_UNIX_ATTRIBUTES_EX ||
       crypto_digest_stream_type(stream) != CRYPTO_DIGEST_NONE) {
      if (!jcr->no_attributes) {
         if (are_attributes_spooled(jcr)) {
            jcr->dir_bsock->spool = true;
         }
         Dmsg0(850, "Send attributes to dir.\n");
         if (!dir_update_file_attributes(jcr->dcr, rec)) {
            jcr->dir_bsock->spool = false;
            Jmsg(jcr, M_FATAL, 0, _("Error updating file attributes. ERR=%s\n"),
               bnet_strerror(jcr->dir_bsock));
            return false;
         }
         jcr->dir_bsock->spool = false;
      }
   }

   return true;
}
