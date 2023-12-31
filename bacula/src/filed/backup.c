/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2022 Kern Sibbald

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
 *  Bacula File Daemon  backup.c  send file attributes and data
 *   to the Storage daemon.
 *
 *    Kern Sibbald, March MM
 */

#include "bacula.h"
#include "filed.h"
#include "backup.h"

#ifdef HAVE_LZO
const bool have_lzo = true;
#else
const bool have_lzo = false;
#endif

#ifdef HAVE_LIBZ
const bool have_libz = true;
#else
const bool have_libz = false;
#endif

/* Forward referenced functions */
int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
static int send_data(bctx_t &bctx, int stream);
static bool encode_and_send_metadata(bctx_t &bctx);
static void close_vss_backup_session(JCR *jcr);
#ifdef HAVE_DARWIN_OS
static bool send_resource_fork(bctx_t &bctx);
#endif
static bool setup_compression(bctx_t &bctx);
static bool do_lzo_compression(bctx_t &bctx);
static bool do_libz_compression(bctx_t &bctx);

/**
 * Find all the requested files and send them
 * to the Storage daemon.
 *
 * Note, we normally carry on a one-way
 * conversation from this point on with the SD, simply blasting
 * data to him.  To properly know what is going on, we
 * also run a "heartbeat" monitor which reads the socket and
 * reacts accordingly (at the moment it has nothing to do
 * except echo the heartbeat to the Director).
 *
 */
bool blast_data_to_storage_daemon(JCR *jcr, char *addr)
{
   BSOCK *sd;
   bool ok = true;

   /* Initialize the poll manager if we have the configuration set */
   if (me->sd_packet_check) {
      jcr->sd_packet_mgr = New(bnet_poll_manager(me->sd_packet_check));
   }

   sd = jcr->store_bsock;

   jcr->setJobStatus(JS_Running);

   Dmsg1(300, "bfiled: opened data connection %d to stored\n", sd->m_fd);

   LockRes();
   CLIENT *client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   uint32_t buf_size;
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!sd->set_buffer_size(buf_size, BNET_SETBUF_WRITE)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      Jmsg(jcr, M_FATAL, 0, _("Cannot set buffer size FD->SD.\n"));
      return false;
   }

   jcr->buf_size = sd->msglen;
   /**
    * Adjust for compression so that output buffer is
    *  12 bytes + 0.1% larger than input buffer plus 18 bytes.
    *  This gives a bit extra plus room for the sparse addr if any.
    *  Note, we adjust the read size to be smaller so that the
    *  same output buffer can be used without growing it.
    *
    *  For LZO1X compression the recommended value is :
    *                  output_block_size = input_block_size + (input_block_size / 16) + 64 + 3 + sizeof(comp_stream_header)
    *
    * The zlib compression workset is initialized here to minimize
    *  the "per file" load. The jcr member is only set, if the init
    *  was successful.
    *
    *  For the same reason, lzo compression is initialized here.
    */
   if (have_lzo) {
      jcr->compress_buf_size = MAX(jcr->buf_size + (jcr->buf_size / 16) + 67 + (int)sizeof(comp_stream_header), jcr->buf_size + ((jcr->buf_size+999) / 1000) + 30);
      jcr->compress_buf = get_memory(jcr->compress_buf_size);
   } else {
      jcr->compress_buf_size = jcr->buf_size + ((jcr->buf_size+999) / 1000) + 30;
      jcr->compress_buf = get_memory(jcr->compress_buf_size);
   }

#ifdef HAVE_LIBZ
   z_stream *pZlibStream = (z_stream*)malloc(sizeof(z_stream));
   if (pZlibStream) {
      pZlibStream->zalloc = Z_NULL;
      pZlibStream->zfree = Z_NULL;
      pZlibStream->opaque = Z_NULL;
      pZlibStream->state = Z_NULL;

      if (deflateInit(pZlibStream, Z_DEFAULT_COMPRESSION) == Z_OK) {
         jcr->pZLIB_compress_workset = pZlibStream;
      } else {
         free (pZlibStream);
      }
   }
#endif

#ifdef HAVE_LZO
   lzo_voidp pLzoMem = (lzo_voidp) malloc(LZO1X_1_MEM_COMPRESS);
   if (pLzoMem) {
      if (lzo_init() == LZO_E_OK) {
         jcr->LZO_compress_workset = pLzoMem;
      } else {
         free (pLzoMem);
      }
   }
#endif

   if (!crypto_session_start(jcr)) {
      return false;
   }

   set_find_options(jcr->ff, jcr->incremental, jcr->mtime);
   set_find_snapshot_function(jcr->ff, snapshot_convert_path);

   /** in accurate mode, we overload the find_one check function */
   if (jcr->accurate) {
      set_find_changed_function((FF_PKT *)jcr->ff, accurate_check_file);
   }
   start_heartbeat_monitor(jcr);

#ifdef HAVE_ACL
   jcr->bacl = (BACL*)new_bacl();
#endif
#ifdef HAVE_XATTR
   jcr->bxattr = (BXATTR*)new_bxattr();
#endif

   if (!dedup_init_storage_bsock(jcr, sd)) {
      return false;
   }

   /** Subroutine save_file() is called for each file */
   if (!find_files(jcr, (FF_PKT *)jcr->ff, save_file, plugin_save)) {
      ok = false;                     /* error */
      jcr->setJobStatus(JS_ErrorTerminated);
   }

#ifdef HAVE_ACL
   if (jcr->bacl && jcr->bacl->get_acl_nr_errors() > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Had %ld acl errors while doing backup\n"),
         jcr->bacl->get_acl_nr_errors());
   }
#endif
#ifdef HAVE_XATTR
   if (jcr->bxattr && jcr->bxattr->get_xattr_nr_errors() > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Had %ld xattr errors while doing backup\n"),
         jcr->bxattr->get_xattr_nr_errors());
   }
#endif
   /* Delete or keep snapshots */
   close_snapshot_backup_session(jcr);
   close_vss_backup_session(jcr);

   accurate_finish(jcr);              /* send deleted or base file list to SD */

   dedup_release_storage_bsock(jcr, sd);

   stop_heartbeat_monitor(jcr);
   sd->signal(BNET_EOD);            /* end of sending data */

#ifdef HAVE_ACL
   if (jcr->bacl) {
      delete(jcr->bacl);
      jcr->bacl = NULL;
   }
#endif
#ifdef HAVE_XATTR
   if (jcr->bxattr) {
      delete(jcr->bxattr);
      jcr->bxattr = NULL;
   }
#endif
   if (jcr->big_buf) {
      bfree_and_null(jcr->big_buf);
   }
   if (jcr->compress_buf) {
      free_and_null_pool_memory(jcr->compress_buf);
   }
   if (jcr->pZLIB_compress_workset) {
      /* Free the zlib stream */
#ifdef HAVE_LIBZ
      deflateEnd((z_stream *)jcr->pZLIB_compress_workset);
#endif
      bfree_and_null(jcr->pZLIB_compress_workset);
   }
   if (jcr->LZO_compress_workset) {
      bfree_and_null(jcr->LZO_compress_workset);
   }

   crypto_session_end(jcr);

   bdelete_and_null(jcr->sd_packet_mgr);

   Dmsg1(100, "end blast_data ok=%d\n", ok);
   return ok;
}

/*
 * Called by plugin_metadata_backup() for each file for which there is
 * metadata provided by the plugin.
 * Using the plugin_metadata class it iterates over metadata packets, serialize it and sends
 * it to the sd one by one.
 *
 *  Returns: true if OK
 *           false if error (size of the meta packet is too big or communication with the sd failed)
 */
bool metadata_save(JCR *jcr, const plugin_metadata *plug_meta)
{
   bool stat = false;
   BSOCK *sd = jcr->store_bsock;
   uint32_t mp_count = plug_meta->count();

   /* Send each metadata packet separately to the sd */
   for (uint32_t i=0; i<mp_count; i++) {
      meta_pkt *mp = plug_meta->get(i);

      if (mp->size() > 3000000) {
         Jmsg1(jcr, M_ERROR, 0, _("Metadata size (%ld) is bigger then currently supported one (3MB)\n"),
                                 mp->size());
         goto bail_out;
      }

      /* Send stream header */
      switch (mp->type) {
         case plugin_meta_blob:
            stat = sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_PLUGIN_META_BLOB);
            if (!stat) {
               if (!jcr->is_canceled() && !jcr->is_incomplete()) {
                  Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                        sd->bstrerror());
               }
               goto bail_out;
            }
            break;
         case plugin_meta_catalog_email:
            stat = sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_PLUGIN_META_CATALOG);
            if (!stat) {
               if (!jcr->is_canceled() && !jcr->is_incomplete()) {
                  Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                        sd->bstrerror());
               }
               goto bail_out;
            }
            break;
         default:
            Jmsg1(jcr, M_FATAL, 0, _("Invalid metadata type: %d\n"), mp->type);
            goto bail_out;
      }

      /* Allocate space to store entire metadata packet as a single buffer. We do not need do differentiate metadata
       * types since each of them uses mp->data as a main metadata payload. */
      sd->msg = check_pool_memory_size(sd->msg, mp->size());
      sd->msglen = mp->size();

      /* Serialize packet */
      mp->serialize(sd->msg);

      /* Send metadata packet */
      stat = sd->send();
      sd->signal(BNET_EOD); /* indicate end of attributes data */
   }

bail_out:
   return stat;
}

/**
 * Called here by find() for each file included.
 *   This is a callback. The original is find_files() above.
 *
 *  Send the file and its data to the Storage daemon.
 *
 *  Returns: 1 if OK
 *           0 if error
 *          -1 to ignore file/directory (not used here)
 */
int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   bool do_read = false;
   bool plugin_started = false;
   bool do_plugin_set = false;
   int stat;
   int rtnstat = 0;
   bool has_file_data = false;
   struct save_pkt sp;          /* used by option plugin */
   BSOCK *sd = jcr->store_bsock;
   bctx_t bctx;                  /* backup context */

   memset(&bctx, 0, sizeof(bctx));
   bctx.sd = sd;
   bctx.ff_pkt = ff_pkt;
   bctx.jcr = jcr;


   time_t now = time(NULL);
   if (jcr->last_stat_time == 0) {
      jcr->last_stat_time = now;
      jcr->stat_interval = 30;  /* Default 30 seconds */
   } else if (now >= jcr->last_stat_time + jcr->stat_interval) {
      jcr->dir_bsock->fsend("Progress JobId=%ld files=%ld bytes=%lld bps=%ld\n",
         jcr->JobId, jcr->JobFiles, jcr->JobBytes, jcr->LastRate);
      jcr->last_stat_time = now;
   }

   if (jcr->is_canceled() || jcr->is_incomplete()) {
      Dmsg0(100, "Job canceled by user or marked incomplete.\n");
      return 0;
   }

   jcr->num_files_examined++;         /* bump total file count */

   switch (ff_pkt->type) {
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
      Dmsg2(130, "FT_LNKSAVED hard link: %s => %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_REGE:
      Dmsg1(130, "FT_REGE saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
   case FT_REG:
      Dmsg1(130, "FT_REG saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
   case FT_LNK:
      Dmsg2(130, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_RESTORE_FIRST:
      Dmsg1(100, "FT_RESTORE_FIRST saving: %s\n", ff_pkt->fname);
      break;
   case FT_PLUGIN_CONFIG:
      Dmsg1(100, "FT_PLUGIN_CONFIG saving: %s\n", ff_pkt->fname);
      break;
   case FT_PLUGIN_OBJECT:
      break;
   case FT_DIRBEGIN:
      jcr->num_files_examined--;      /* correct file count */
      return 1;                       /* not used */
   case FT_NORECURSE:
      Jmsg(jcr, M_INFO, 1, _("     Recursion turned off. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_NOFSCHG:
      /* Suppress message for /dev filesystems */
      if (!is_in_fileset(ff_pkt)) {
         Jmsg(jcr, M_INFO, 1, _("     %s is a different filesystem. Will not descend from %s into it.\n"),
              ff_pkt->fname, ff_pkt->top_fname);
      }
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_INVALIDFS:
      Jmsg(jcr, M_INFO, 1, _("     Disallowed filesystem. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_INVALIDDT:
      Jmsg(jcr, M_INFO, 1, _("     Disallowed drive type. Will not descend into %s\n"),
           ff_pkt->fname);
      break;
   case FT_REPARSE:
   case FT_JUNCTION:
   case FT_DIREND:
      Dmsg1(130, "FT_DIREND: %s\n", ff_pkt->link);
      break;
   case FT_SPEC:
      Dmsg1(130, "FT_SPEC saving: %s\n", ff_pkt->fname);
      if (S_ISSOCK(ff_pkt->statp.st_mode)) {
        Jmsg(jcr, M_SKIPPED, 1, _("     Socket file skipped: %s\n"), ff_pkt->fname);
        return 1;
      }
      break;
   case FT_RAW:
      Dmsg1(130, "FT_RAW saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
   case FT_FIFO:
      Dmsg1(130, "FT_FIFO saving: %s\n", ff_pkt->fname);
      break;
   case FT_NOACCESS: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not access \"%s\": ERR=%s\n"), ff_pkt->fname,
         be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOFOLLOW: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not follow link \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOSTAT: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not stat \"%s\": ERR=%s\n"), ff_pkt->fname,
         be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_DIRNOCHG:
   case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, 1, _("     Unchanged file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_ISARCH:
      Jmsg(jcr, M_NOTSAVED, 0, _("     Archive file not saved: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NOOPEN: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not open directory \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_DELETED:
      Dmsg1(130, "FT_DELETED: %s\n", ff_pkt->fname);
      break;
   default:
      Jmsg(jcr, M_NOTSAVED, 0,  _("     Unknown file type %d; not saved: %s\n"),
           ff_pkt->type, ff_pkt->fname);
      jcr->JobErrors++;
      return 1;
   }

   Dmsg1(130, "bfiled: sending %s to stored\n", ff_pkt->fname);

   /** Digests and encryption are only useful if there's file data */
   if (has_file_data && !crypto_setup_digests(bctx)) {
      goto good_rtn;
   }

   /** Initialize the file descriptor we use for data and other streams. */
   binit(&ff_pkt->bfd);

   if (ff_pkt->cmd_plugin) {
      do_plugin_set = true;

   /* option and cmd plugin are not compatible together */
   } else if (ff_pkt->opt_plugin) {

      /* ask the option plugin what to do with this file */
      switch (plugin_option_handle_file(jcr, ff_pkt, &sp)) {
      case bRC_OK:
         Dmsg2(10, "Option plugin %s will be used to backup %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         do_plugin_set = true;
         break;
      case bRC_Skip:
         Dmsg2(10, "Option plugin %s decided to skip %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         goto good_rtn;
      default:
         Dmsg2(10, "Option plugin %s decided to let bacula handle %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         break;
      }
   }

   if (ff_pkt->flags & FO_PORTABLE) {
      set_portable_backup(&ff_pkt->bfd); /* disable Win32 BackupRead() */
   }

   if (do_plugin_set) {
      /* Tell bfile that it needs to call plugin */
      if (!set_cmd_plugin(&ff_pkt->bfd, jcr)) {
         goto bail_out;
      }
      send_plugin_name(jcr, sd, true);      /* signal start of plugin data */
      plugin_started = true;
   }

   /*
    * Send attributes -- must be done after binit()
    *  Note: this subroutine also defines bctx.stream
    */
   if (!encode_and_send_attributes(bctx)) {
      goto bail_out;
   }

   /* Send Plugin Metadata attributes. Available only for Plugins */
   if (!encode_and_send_metadata(bctx)) {
      goto bail_out;
   }

   if (IS_FT_OBJECT(ff_pkt->type) || /* Meta data only for restore object */
       ff_pkt->type == FT_DELETED){  /* Meta data only for deleted files */
      goto good_rtn;
   }

   /* If we have only to update attributes, we need to send the checksum to fill
    * the catalog correctly for the next job
    */
   if (ff_pkt->stat_update) {
      has_file_data = false;
   }

   /** Set up the encryption context and send the session data to the SD */
   if (has_file_data && jcr->crypto.pki_encrypt) {
      if (!crypto_session_send(jcr, sd)) {
         goto bail_out;
      }
   }

   /**
    * Open any file with data that we intend to save, then save it.
    *
    * Note, if is_win32_backup, we must open the Directory so that
    * the BackupRead will save its permissions and ownership streams.
    */
   if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode)) {
#ifdef HAVE_WIN32
      do_read = !is_portable_backup(&ff_pkt->bfd) || ff_pkt->statp.st_size > 0;
#else
      do_read = ff_pkt->statp.st_size > 0;
#endif
   } else if (ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO ||
              ff_pkt->type == FT_REPARSE || ff_pkt->type == FT_JUNCTION ||
         (!is_portable_backup(&ff_pkt->bfd) && ff_pkt->type == FT_DIREND)) {
      do_read = true;
   }

   /* If we have only to update attributes, we need to send the checksum to fill
    * the catalog correctly for the next job
    */
   if (ff_pkt->stat_update) {
      do_read = false;
   }

   if (ff_pkt->cmd_plugin && !ff_pkt->no_read) {
      do_read = true;
   }

   Dmsg2(150, "type=%d do_read=%d\n", ff_pkt->type, do_read);
   if (do_read) {
      btimer_t *tid;

      if (ff_pkt->type == FT_FIFO) {
         tid = start_thread_timer(jcr, pthread_self(), 60);
      } else {
         tid = NULL;
      }
      int noatime = ff_pkt->flags & FO_NOATIME ? O_NOATIME : 0;
      ff_pkt->bfd.reparse_point = (ff_pkt->type == FT_REPARSE ||
                                   ff_pkt->type == FT_JUNCTION);
      set_fattrs(&ff_pkt->bfd, &ff_pkt->statp);
      if (bopen(&ff_pkt->bfd, ff_pkt->snap_fname, O_RDONLY | O_BINARY | noatime, 0) < 0) {
         ff_pkt->ff_errno = errno;
         berrno be;
         Jmsg(jcr, M_NOTSAVED, 0, _("     Cannot open \"%s\": ERR=%s.\n"), ff_pkt->snap_fname,
              be.bstrerror());
         jcr->JobErrors++;
         if (tid) {
            stop_thread_timer(tid);
            tid = NULL;
         }
         goto good_rtn;
      }
      if (tid) {
         stop_thread_timer(tid);
         tid = NULL;
      }

      stat = send_data(bctx, bctx.data_stream);

      if (ff_pkt->flags & FO_CHKCHANGES) {
         has_file_changed(jcr, ff_pkt);
      }

      bclose(&ff_pkt->bfd);

      if (!stat) {
         goto bail_out;
      }
   }

#ifdef HAVE_DARWIN_OS
   if (!send_resource_fork(bctx)) {
      goto bail_out;
   }
#endif

   /*
    * Save ACLs and Extended Attributes when requested and available
    * for anything not being a symlink.
    */
#ifdef HAVE_ACL
   if (jcr->bacl && jcr->bacl->backup_acl(jcr, ff_pkt) != bRC_BACL_ok) {
      goto bail_out;
   }
#endif
#ifdef HAVE_XATTR
   if (jcr->bxattr && jcr->bxattr->backup_xattr(jcr, ff_pkt) != bRC_BXATTR_ok) {
      goto bail_out;
   }
#endif

   if (!crypto_terminate_digests(bctx)) {
      goto bail_out;
   }

good_rtn:
   rtnstat = 1;

bail_out:
   if (jcr->is_incomplete() || jcr->is_canceled()) {
      Dmsg0(100, "Job canceled by user or marked incomplete.\n");
      rtnstat = 0;
   }
   if (plugin_started) {
      send_plugin_name(jcr, sd, false); /* signal end of plugin data */
   }
   if (ff_pkt->opt_plugin) {
      jcr->plugin_sp = NULL;    /* sp is local to this function */
      jcr->plugin_ctx = NULL;
      jcr->plugin = NULL;
      jcr->opt_plugin = false;
   }
   crypto_free(bctx);
   return rtnstat;
}

/**
 * Send data read from an already open file descriptor.
 *
 * We return 1 on success and 0 on errors.
 *
 * ***FIXME***
 * We use ff_pkt->statp.st_size when FO_SPARSE to know when to stop
 *  reading.
 * Currently this is not a problem as the only other stream, resource forks,
 * are not handled as sparse files.
 */
static int send_data(bctx_t &bctx, int stream)
{
   JCR *jcr = bctx.jcr;
   BSOCK *sd = jcr->store_bsock;

#ifdef FD_NO_SEND_TEST
   return 1;
#endif

   bctx.rsize = jcr->buf_size;
   bctx.fileAddr = 0;
   bctx.cipher_ctx = NULL;
   bctx.msgsave = sd->msg;
   bctx.rbuf = sd->msg;                    /* read buffer */
   bctx.wbuf = sd->msg;                    /* write buffer */
   bctx.cipher_input = (uint8_t *)bctx.rbuf;    /* encrypt uncompressed data */

   Dmsg1(300, "Saving data, type=%d\n", bctx.ff_pkt->type);

   if (!setup_compression(bctx)) {
      goto err;
   }

   if (bctx.ff_pkt->flags & FO_ENCRYPT && !crypto_allocate_ctx(bctx)) {
      return false;
   }

   if (bctx.ff_pkt->flags & FO_DEDUPLICATION) {
      if (jcr->sd_dedup == 0 ) {
         /* the SD (the device) cannot do dedup then ignore the directive */
      } else if (bctx.ff_pkt->Dedup_level == 2) { // 2 == BothSides
         if (is_deduplicable_stream(stream) &&
             (bctx.ff_pkt->statp.st_size >= jcr->min_dedup_block_size || bctx.ff_pkt->statp.st_size == 0)) {
           /* plugins often wrongly use -1, for a unsigned this is a big number that
            * must satisfy the first test, 0 can be accepted too, because if this is
            * a true 0 then no blocks should come else this go into the dedupengine */
            Dmsg1(DT_DEDUP|425, "Do client side dedup on stream 0x%x\n", stream);
            stream |= STREAM_BIT_DEDUPLICATION_DATA;
            bctx.dedup_client_side = true;
         } else {
            // Don't do dedup on this unfriendly dedup stream
            Dmsg2(DT_DEDUP|425, "No dedup on unfriendly dedup stream 0x%x or file too small %lld\n", stream, bctx.ff_pkt->statp.st_size);
         }
      } else if (bctx.ff_pkt->Dedup_level == 0) { // 0 == none
         // Deny dedup on that stream
         stream |= STREAM_BIT_NO_DEDUPLICATION;
         Dmsg1(DT_DEDUP|425, "Deny dedup on this stream 0x%x\n", stream);
      } else { // bctx.ff_pkt->Dedup_level == 1 let the SD do dedup

      }
   }

   /* Check if we want to send the offset information with each block */
   if (bctx.ff_pkt->flags & FO_OFFSETS) {
      stream |= STREAM_BIT_OFFSETS;
   }

   /*
    * Send Data header to Storage daemon
    *    <file-index> <stream> <expected stream length>
    */
   if (!sd->fsend("%ld %d %lld", jcr->JobFiles, stream,
        (int64_t)bctx.ff_pkt->statp.st_size)) {
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      goto err;
   }
   Dmsg1(300, ">stored: datahdr %s\n", sd->msg);

   /**
    * Make space at beginning of buffer for fileAddr because this
    *   same buffer will be used for writing if compression is off.
    */
   if ((bctx.ff_pkt->flags & FO_SPARSE) || (bctx.ff_pkt->flags & FO_OFFSETS)) {
      bctx.rbuf += OFFSET_FADDR_SIZE;
      /* With FO_OFFSET we want to send 64K buffers + the pointer to the current
       * offset inside the current file. It is important for Aligned volumes.
       *
       * With SPARSE or Dedup+FO_OFFSET, we can continue with data that is
       * slightly smaller. (To keep aligned/dedup chunk identical with the ones
       * stored by previous releases we must must decrease rsize)
       */
      if (bctx.ff_pkt->flags & FO_SPARSE || jcr->sd_dedup) {
         bctx.rsize -= OFFSET_FADDR_SIZE;
      }

#if defined(HAVE_FREEBSD_OS) || defined(__FreeBSD_kernel__)
      /**
       * To read FreeBSD partitions, the read size must be
       *  a multiple of 512.
       */
      bctx.rsize = (bctx.rsize/512) * 512;
#endif
   }

   /** a RAW device read on win32 only works if the buffer is a multiple of 512 */
#ifdef HAVE_WIN32
   if (S_ISBLK(bctx.ff_pkt->statp.st_mode)) {
      bctx.rsize = (bctx.rsize/512) * 512;
   }
   Dmsg1(200, "Fattrs=0X%x\n", bctx.ff_pkt->bfd.fattrs);
   if (bctx.ff_pkt->bfd.fattrs & FILE_ATTRIBUTE_ENCRYPTED) {
      if (!p_ReadEncryptedFileRaw) {
         Jmsg0(bctx.jcr, M_FATAL, 0, _("Windows Encrypted data not supported on this OS.\n"));
         goto err;
      }
      /* This single call reads all EFS data delivers it to a callback */
      if (p_ReadEncryptedFileRaw((PFE_EXPORT_FUNC)read_efs_data_cb, &bctx,
            bctx.ff_pkt->bfd.pvContext) != 0) {
         goto err;
      }
      /* All read, so skip to finish sending */
      goto finish_sending;
   }
   /* Fall through to standard bread() loop */
#endif

   /*
    * Normal read the file data in a loop and send it to SD
    */
   while ((sd->msglen=(uint32_t)bread(&bctx.ff_pkt->bfd, bctx.rbuf, bctx.rsize)) > 0) {
      if (!process_and_send_data(bctx)) {
         goto err;
      }
      if (jcr->sd_packet_mgr) {
         jcr->sd_packet_mgr->send(jcr, sd); // Send a POLL request if needed
      }
   } /* end while read file data */
   goto finish_sending;

finish_sending:
   if (sd->msglen < 0) {                 /* error */
      berrno be;
      Jmsg(jcr, M_ERROR, 0, _("Read error on file %s. ERR=%s\n"),
         bctx.ff_pkt->snap_fname, be.bstrerror(bctx.ff_pkt->bfd.berrno));
      /* It's possible for user to set max job errors to 0 as an infinite value */
      if (me->max_job_errors && (jcr->JobErrors++ > me->max_job_errors)) {       /* insanity check */
         Jmsg(jcr, M_FATAL, 0, _("Too many errors. JobErrors=%d.\n"), jcr->JobErrors);
      }
   } else if (bctx.ff_pkt->flags & FO_ENCRYPT) {
      /**
       * For encryption, we must call finalize to push out any
       *  buffered data.
       */
      if (!crypto_cipher_finalize(bctx.cipher_ctx, (uint8_t *)jcr->crypto.crypto_buf,
           &bctx.encrypted_len)) {
         /* Padding failed. Shouldn't happen. */
         Jmsg(jcr, M_FATAL, 0, _("Encryption padding error\n"));
         goto err;
      }

      /** Note, on SSL pre-0.9.7, there is always some output */
      if (bctx.encrypted_len > 0) {
         sd->msglen = bctx.encrypted_len;     /* set encrypted length */
         sd->msg = jcr->crypto.crypto_buf;    /* set correct write buffer */
         if (!sd->send()) {
            if (!jcr->is_job_canceled()) {
               Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                     sd->bstrerror());
            }
            goto err;
         }
         Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
         jcr->JobBytes += sd->msglen;     /* count bytes saved possibly compressed/encrypted */
         sd->msg = bctx.msgsave;          /* restore bnet buffer */
      }
   }

   jcr->dedup->TransferJobBytes(&jcr->JobBytes); // transfer byte from

   if (!sd->signal(BNET_EOD)) {        /* indicate end of file data */
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      goto err;
   }

   /** Free the cipher context */
   if (bctx.cipher_ctx) {
      crypto_cipher_free(bctx.cipher_ctx);
   }
   return 1;

err:
   /** Free the cipher context */
   if (bctx.cipher_ctx) {
      crypto_cipher_free(bctx.cipher_ctx);
   }

   sd->msg = bctx.msgsave; /* restore bnet buffer */
   sd->msglen = 0;
   return 0;
}


/*
 * Apply processing (sparse, compression, encryption, and
 *   send to the SD.
 */
bool process_and_send_data(bctx_t &bctx)
{
   bool  ret = false;
   BSOCK *sd = bctx.sd;
   JCR *jcr = bctx.jcr;

   Dmsg5(DT_DEDUP|620, "bread msglen=%5d data=0x%08x flags=0x%x sparse=%d compress=%d\n",
         sd->msglen, hash2int(bctx.rbuf), bctx.ff_pkt->flags,
         (bctx.ff_pkt->flags & FO_SPARSE)?1:0,
         (bctx.ff_pkt->flags & FO_COMPRESS)?1:0);

   /** Check for sparse blocks */
   if (bctx.ff_pkt->flags & FO_SPARSE) {
      ser_declare;
      bool allZeros = false;
      if ((sd->msglen == bctx.rsize &&
           bctx.fileAddr+sd->msglen < (uint64_t)bctx.ff_pkt->statp.st_size) ||
          ((bctx.ff_pkt->type == FT_RAW || bctx.ff_pkt->type == FT_FIFO) &&
            (uint64_t)bctx.ff_pkt->statp.st_size == 0)) {
         allZeros = is_buf_zero(bctx.rbuf, bctx.rsize);
      }
      if (!allZeros) {
         /** Put file address as first data in buffer */
         ser_begin(bctx.wbuf, OFFSET_FADDR_SIZE);
         ser_uint64(bctx.fileAddr);     /* store fileAddr in begin of buffer */
      }
      bctx.fileAddr += sd->msglen;      /* update file address */
      /** Skip block of all zeros */
      if (allZeros) {
         ret = true;
         goto err;               /* skip block of zeros */
      }
   } else if (bctx.ff_pkt->flags & FO_OFFSETS) {
      ser_declare;
      ser_begin(bctx.wbuf, OFFSET_FADDR_SIZE);
      ser_uint64(bctx.ff_pkt->bfd.offset);     /* store offset in begin of buffer */
   }

   jcr->ReadBytes += sd->msglen;         /* count bytes read */

   /* Debug code: check if we must hangup or blowup */
   if (handle_hangup_blowup(jcr, 0, jcr->ReadBytes)) {
      goto err;
   }

   /** Uncompressed cipher input length */
   bctx.cipher_input_len = sd->msglen;

   /** Update checksum if requested */
   if (bctx.digest) {
      crypto_digest_update(bctx.digest, (uint8_t *)bctx.rbuf, sd->msglen);
   }

   /** Update signing digest if requested */
   if (bctx.signing_digest) {
      crypto_digest_update(bctx.signing_digest, (uint8_t *)bctx.rbuf, sd->msglen);
   }

   if (have_libz && !do_libz_compression(bctx)) {
      goto err;
   }

   if (have_lzo && !do_lzo_compression(bctx)) {
      goto err;
   }

   /**
    * Note, here we prepend the current record length to the beginning
    *  of the encrypted data. This is because both sparse and compression
    *  restore handling want records returned to them with exactly the
    *  same number of bytes that were processed in the backup handling.
    *  That is, both are block filters rather than a stream.  When doing
    *  compression, the compression routines may buffer data, so that for
    *  any one record compressed, when it is decompressed the same size
    *  will not be obtained. Of course, the buffered data eventually comes
    *  out in subsequent crypto_cipher_update() calls or at least
    *  when crypto_cipher_finalize() is called.  Unfortunately, this
    *  "feature" of encryption enormously complicates the restore code.
    */
   if (bctx.ff_pkt->flags & FO_ENCRYPT) {
      uint32_t initial_len = 0;
      ser_declare;

      if ((bctx.ff_pkt->flags & FO_SPARSE) || (bctx.ff_pkt->flags & FO_OFFSETS)) {
         bctx.cipher_input_len += OFFSET_FADDR_SIZE;
      }

      /** Encrypt the length of the input block */
      uint8_t packet_len[sizeof(uint32_t)];

      ser_begin(packet_len, sizeof(uint32_t));
      ser_uint32(bctx.cipher_input_len);    /* store data len in begin of buffer */
      Dmsg1(20, "Encrypt len=%d\n", bctx.cipher_input_len);

      if (!crypto_cipher_update(bctx.cipher_ctx, packet_len, sizeof(packet_len),
          (uint8_t *)jcr->crypto.crypto_buf, &initial_len)) {
         /** Encryption failed. Shouldn't happen. */
         Jmsg(jcr, M_FATAL, 0, _("Encryption error\n"));
         goto err;
      }

      /** Encrypt the input block */
      if (crypto_cipher_update(bctx.cipher_ctx, bctx.cipher_input, bctx.cipher_input_len,
          (uint8_t *)&jcr->crypto.crypto_buf[initial_len], &bctx.encrypted_len)) {
         if ((initial_len + bctx.encrypted_len) == 0) {
            /** No full block of data available, read more data */
            ret = true;
            goto err;
         }
         Dmsg2(400, "encrypted len=%d unencrypted len=%d\n", bctx.encrypted_len,
               sd->msglen);
         sd->msglen = initial_len + bctx.encrypted_len; /* set encrypted length */
      } else {
         /** Encryption failed. Shouldn't happen. */
         Jmsg(jcr, M_FATAL, 0, _("Encryption error\n"));
         goto err;
      }
   }

   if (bctx.dedup_client_side) {
      if (!do_dedup_client_side(bctx)) {
         goto err;
      }
   }

   /* Send the buffer to the Storage daemon */
   if ((bctx.ff_pkt->flags & FO_SPARSE) || (bctx.ff_pkt->flags & FO_OFFSETS)) {
      sd->msglen += OFFSET_FADDR_SIZE; /* include fileAddr in size */
   }
   sd->msg = bctx.wbuf;              /* set correct write buffer */
   if (!sd->send()) {
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      goto err;
   }
   Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
   /*          #endif */
   jcr->JobBytes += sd->msglen;      /* count bytes saved possibly compressed/encrypted */
   sd->msg = bctx.msgsave;                /* restore read buffer */
   ret = true;

err:
   return ret;
}

static bool encode_and_send_metadata(bctx_t &bctx)
{
   if (!bctx.ff_pkt->cmd_plugin) {
      return true;
   }

   return plugin_backup_metadata(bctx.jcr, bctx.ff_pkt);
}

bool encode_and_send_attributes(bctx_t &bctx)
{
   BSOCK *sd = bctx.jcr->store_bsock;
   JCR *jcr = bctx.jcr;
   FF_PKT *ff_pkt = bctx.ff_pkt;
   char attribs[MAXSTRING];
   char attribsExBuf[MAXSTRING];
   char *attribsEx = NULL;
   int attr_stream;
   int comp_len;
   bool stat;
#ifdef FD_NO_SEND_TEST
   return true;
#endif

   Dmsg1(300, "encode_and_send_attrs fname=%s\n", ff_pkt->fname);
   /** Find what data stream we will use, then encode the attributes */
   if ((bctx.data_stream = select_data_stream(ff_pkt)) == STREAM_NONE) {
      /* This should not happen */
      Jmsg0(jcr, M_FATAL, 0, _("Invalid file flags, no supported data stream type.\n"));
      return false;
   }
   encode_stat(attribs, &ff_pkt->statp, sizeof(ff_pkt->statp), ff_pkt->LinkFI, bctx.data_stream);

   /** Now possibly extend the attributes */
   if (ff_pkt->type != FT_PLUGIN_OBJECT && IS_FT_OBJECT(ff_pkt->type)) {
      attr_stream = STREAM_RESTORE_OBJECT;
   } else if (ff_pkt->type == FT_PLUGIN_OBJECT) {
      attr_stream = STREAM_PLUGIN_OBJECT;
   } else if (ff_pkt->stat_update) {
      attr_stream = STREAM_UNIX_ATTRIBUTE_UPDATE;
   } else {
      attribsEx = attribsExBuf;
      attr_stream = encode_attribsEx(jcr, attribsEx, ff_pkt);
   }

   Dmsg3(300, "File %s\nattribs=%s\nattribsEx=%s\n", ff_pkt->fname, attribs, attribsEx);

   jcr->lock();
   jcr->JobFiles++;                    /* increment number of files sent */
   ff_pkt->FileIndex = jcr->JobFiles;  /* return FileIndex */
   pm_strcpy(jcr->last_fname, ff_pkt->fname);
   jcr->unlock();

   /* Display the information about the current file if requested */
   if (is_message_type_set(jcr, M_SAVED)) {
      ATTR attr;
      memcpy(&attr.statp, &ff_pkt->statp, sizeof(struct stat));
      attr.type = ff_pkt->type;
      attr.ofname = (POOLMEM *)ff_pkt->fname;
      attr.olname = (POOLMEM *)ff_pkt->link;
      print_ls_output(jcr, &attr, M_SAVED);
   }

   /* Debug code: check if we must hangup or blowup */
   if (handle_hangup_blowup(jcr, jcr->JobFiles, 0)) {
      return false;
   }

   /*
    * Send Attributes header to Storage daemon
    *    <file-index> <stream> <info>
    */
   if (!sd->fsend("%ld %d 0", jcr->JobFiles, attr_stream)) {
      if (!jcr->is_canceled() && !jcr->is_incomplete()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      return false;
   }
   Dmsg1(300, ">stored: attrhdr %s\n", sd->msg);

   /*
    * Send file attributes to Storage daemon
    *   File_index
    *   File type
    *   Filename (full path)
    *   Encoded attributes
    *   Link name (if type==FT_LNK or FT_LNKSAVED)
    *   Encoded extended-attributes (for Win32)
    *
    * or send Restore Object to Storage daemon
    *   File_index
    *   File_type
    *   Object_index
    *   Object_len  (possibly compressed)
    *   Object_full_len (not compressed)
    *   Object_compression
    *   Plugin_name
    *   Object_name
    *   Binary Object data
    *
    * For a directory, link is the same as fname, but with trailing
    * slash. For a linked file, link is the link.
    */
   if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) { /* already stripped */
      strip_path(ff_pkt);
   }
   switch (ff_pkt->type) {
   case FT_LNK:
   case FT_LNKSAVED:
      Dmsg3(300, "Link %d %s to %s\n", jcr->JobFiles, ff_pkt->fname, ff_pkt->link);
      stat = sd->fsend("%ld %d %s%c%s%c%s%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->fname, 0, attribs, 0,
                       ff_pkt->link, 0, attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_DIREND:
   case FT_REPARSE:
   case FT_JUNCTION:
      /* Here link is the canonical filename (i.e. with trailing slash) */
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->link, 0, attribs, 0, 0,
                       attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_PLUGIN_CONFIG:
   case FT_RESTORE_FIRST:
      comp_len = ff_pkt->restore_obj.object_len;
      ff_pkt->restore_obj.object_compression = 0;
      if (ff_pkt->restore_obj.object_len > 1000) {
         /* Big object, compress it */
         comp_len = ff_pkt->restore_obj.object_len + 1000;
         POOLMEM *comp_obj = get_memory(comp_len);
         /* *** FIXME *** check Zdeflate error */
         Zdeflate(ff_pkt->restore_obj.object, ff_pkt->restore_obj.object_len, comp_obj, comp_len);
         if (comp_len < ff_pkt->restore_obj.object_len) {
            ff_pkt->restore_obj.object = comp_obj;
            ff_pkt->restore_obj.object_compression = 1;    /* zlib level 9 compression */
         } else {
            /* Uncompressed object smaller, use it */
            comp_len = ff_pkt->restore_obj.object_len;
         }
         Dmsg2(100, "Object compressed from %d to %d bytes\n", ff_pkt->restore_obj.object_len, comp_len);
      }
      sd->msglen = Mmsg(sd->msg, "%d %d %d %d %d %d %s%c%s%c",
                        jcr->JobFiles, ff_pkt->type, ff_pkt->restore_obj.index,
                        comp_len, ff_pkt->restore_obj.object_len, ff_pkt->restore_obj.object_compression,
                        ff_pkt->fname, 0, ff_pkt->restore_obj.object_name, 0);
      sd->msg = check_pool_memory_size(sd->msg, sd->msglen + comp_len + 2);
      memcpy(sd->msg + sd->msglen, ff_pkt->restore_obj.object, comp_len);
      /* Note send an extra byte so the SD can store zero after object */
      sd->msglen += comp_len + 1;
      stat = sd->send();
      if (ff_pkt->restore_obj.object_compression) {
         free_and_null_pool_memory(ff_pkt->restore_obj.object);
      }
      break;
   case FT_PLUGIN_OBJECT:
      {
         POOL_MEM path;
         POOL_MEM plugin_name;
         POOL_MEM object_category;
         POOL_MEM object_type;
         POOL_MEM object_name;
         POOL_MEM object_source;
         POOL_MEM object_uuid;

         pm_strcpy(path, NPRTB(ff_pkt->plugin_obj.path));
         bash_spaces(path.c_str());
         pm_strcpy(plugin_name, NPRTB(ff_pkt->plugin_obj.plugin_name));
         bash_spaces(plugin_name.c_str());
         pm_strcpy(object_type, NPRTB(ff_pkt->plugin_obj.object_type));
         bash_spaces(object_type.c_str());
         pm_strcpy(object_category, NPRTB(ff_pkt->plugin_obj.object_category));
         bash_spaces(object_category.c_str());
         pm_strcpy(object_name, NPRTB(ff_pkt->plugin_obj.object_name));
         bash_spaces(object_name.c_str());
         pm_strcpy(object_source, NPRTB(ff_pkt->plugin_obj.object_source));
         bash_spaces(object_source.c_str());
         pm_strcpy(object_uuid, NPRTB(ff_pkt->plugin_obj.object_uuid));
         bash_spaces(object_uuid.c_str());
         sd->msglen = Mmsg(sd->msg, "%s %s %s %s %s %s %s %llu %c %lu%c",
                           path.c_str(), plugin_name.c_str(), object_category.c_str(), object_type.c_str(),
                           object_name.c_str(), object_source.c_str(), object_uuid.c_str(),
                           ff_pkt->plugin_obj.object_size,
                           /* Plugin can left status unset, we need to handle that */
                           ff_pkt->plugin_obj.status != 0 ? (char)ff_pkt->plugin_obj.status : 'U',
                           ff_pkt->plugin_obj.count,
                           0);
         stat = sd->send();

         break;
      }
   case FT_REG:
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%d%c", jcr->JobFiles,
               ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0, attribsEx, 0,
               /*TODO we may want to increment the delta_seq number intead of hardcode it to 1,
                * when at some point we start to generate delta sequences also for regular files */
               ff_pkt->stat_update ? 1 : ff_pkt->delta_seq, 0);
      break;
   default:
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0,
                       attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   }

   if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) {
      unstrip_path(ff_pkt);
   }

   char b[1024];
   Dmsg2(300, ">stored: attr len=%d: %s\n", sd->msglen, asciidump(sd->msg, sd->msglen, b, sizeof(b)));
   if (!stat && !jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
   }
   sd->signal(BNET_EOD);            /* indicate end of attributes data */
   return stat;
}

/*
 * Setup bctx for doing compression
 */
static bool setup_compression(bctx_t &bctx)
{
   JCR *jcr = bctx.jcr;

#if defined(HAVE_LIBZ) || defined(HAVE_LZO)
   bctx.compress_len = 0;
   bctx.max_compress_len = 0;
   bctx.cbuf = NULL;
 #ifdef HAVE_LIBZ
   int zstat;

   if ((bctx.ff_pkt->flags & FO_COMPRESS) && bctx.ff_pkt->Compress_algo == COMPRESS_GZIP) {
      if ((bctx.ff_pkt->flags & FO_SPARSE) || (bctx.ff_pkt->flags & FO_OFFSETS)) {
         bctx.cbuf = (unsigned char *)jcr->compress_buf + OFFSET_FADDR_SIZE;
         bctx.max_compress_len = jcr->compress_buf_size - OFFSET_FADDR_SIZE;
      } else {
         bctx.cbuf = (unsigned char *)jcr->compress_buf;
         bctx.max_compress_len = jcr->compress_buf_size; /* set max length */
      }
      bctx.wbuf = jcr->compress_buf;    /* compressed output here */
      bctx.cipher_input = (uint8_t *)jcr->compress_buf; /* encrypt compressed data */

      /**
       * Only change zlib parameters if there is no pending operation.
       * This should never happen as deflatereset is called after each
       * deflate.
       */

      if (((z_stream*)jcr->pZLIB_compress_workset)->total_in == 0) {
         /** set gzip compression level - must be done per file */
         if ((zstat=deflateParams((z_stream*)jcr->pZLIB_compress_workset,
              bctx.ff_pkt->Compress_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
            Jmsg(jcr, M_FATAL, 0, _("Compression deflateParams error: %d\n"), zstat);
            jcr->setJobStatus(JS_ErrorTerminated);
            return false;
         }
      }
   }
 #endif
 #ifdef HAVE_LZO
   memset(&bctx.ch, 0, sizeof(comp_stream_header));
   bctx.cbuf2 = NULL;

   if ((bctx.ff_pkt->flags & FO_COMPRESS) && bctx.ff_pkt->Compress_algo == COMPRESS_LZO1X) {
      if ((bctx.ff_pkt->flags & FO_SPARSE) || (bctx.ff_pkt->flags & FO_OFFSETS)) {
         bctx.cbuf = (unsigned char *)jcr->compress_buf + OFFSET_FADDR_SIZE;
         bctx.cbuf2 = (unsigned char *)jcr->compress_buf + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
         bctx.max_compress_len = jcr->compress_buf_size - OFFSET_FADDR_SIZE;
      } else {
         bctx.cbuf = (unsigned char *)jcr->compress_buf;
         bctx.cbuf2 = (unsigned char *)jcr->compress_buf + sizeof(comp_stream_header);
         bctx.max_compress_len = jcr->compress_buf_size; /* set max length */
      }
      bctx.ch.magic = COMPRESS_LZO1X;
      bctx.ch.version = COMP_HEAD_VERSION;
      bctx.wbuf = jcr->compress_buf;    /* compressed output here */
      bctx.cipher_input = (uint8_t *)jcr->compress_buf; /* encrypt compressed data */
   }
 #endif
#endif
   return true;
}

/*
 * Send MacOS resource fork to SD
 */
#ifdef HAVE_DARWIN_OS
static bool send_resource_fork(bctx_t &bctx)
{
   FF_PKT *ff_pkt = bctx.ff_pkt;
   JCR *jcr = bctx.jcr;
   BSOCK *sd = bctx.sd;
   int stat;

   /** Regular files can have resource forks and Finder Info */
   if (ff_pkt->type != FT_LNKSAVED && (S_ISREG(ff_pkt->statp.st_mode) &&
       ff_pkt->flags & FO_HFSPLUS)) {
      if (ff_pkt->hfsinfo.rsrclength > 0) {
         int flags;
         int rsrc_stream;
         if (bopen_rsrc(&ff_pkt->bfd, ff_pkt->fname, O_RDONLY | O_BINARY, 0) < 0) {
            ff_pkt->ff_errno = errno;
            berrno be;
            Jmsg(jcr, M_NOTSAVED, -1, _("     Cannot open resource fork for \"%s\": ERR=%s.\n"),
                 ff_pkt->fname, be.bstrerror());
            jcr->JobErrors++;
            if (is_bopen(&ff_pkt->bfd)) {
               bclose(&ff_pkt->bfd);
            }
            return true;
         }
         flags = ff_pkt->flags;
         ff_pkt->flags &= ~(FO_COMPRESS|FO_SPARSE|FO_OFFSETS);
         if (flags & FO_ENCRYPT) {
            rsrc_stream = STREAM_ENCRYPTED_MACOS_FORK_DATA;
         } else {
            rsrc_stream = STREAM_MACOS_FORK_DATA;
         }
         stat = send_data(bctx, rsrc_stream);
         ff_pkt->flags = flags;
         bclose(&ff_pkt->bfd);
         if (!stat) {
            return false;
         }
      }

      Dmsg1(300, "Saving Finder Info for \"%s\"\n", ff_pkt->fname);
      sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_HFSPLUS_ATTRIBUTES);
      Dmsg1(300, "bfiled>stored:header %s\n", sd->msg);
      pm_memcpy(sd->msg, ff_pkt->hfsinfo.fndrinfo, 32);
      sd->msglen = 32;
      if (bctx.digest) {
         crypto_digest_update(bctx.digest, (uint8_t *)sd->msg, sd->msglen);
      }
      if (bctx.signing_digest) {
         crypto_digest_update(bctx.signing_digest, (uint8_t *)sd->msg, sd->msglen);
      }
      sd->send();
      sd->signal(BNET_EOD);
   }
   return true;
}
#endif

static bool do_libz_compression(bctx_t &bctx)
{
#ifdef HAVE_LIBZ
   JCR *jcr = bctx.jcr;
   BSOCK *sd = bctx.sd;
   int zstat;

   /** Do compression if turned on */
   if (bctx.ff_pkt->flags & FO_COMPRESS && bctx.ff_pkt->Compress_algo == COMPRESS_GZIP && jcr->pZLIB_compress_workset) {
      Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", bctx.cbuf, bctx.rbuf, sd->msglen);

      ((z_stream*)jcr->pZLIB_compress_workset)->next_in   = (unsigned char *)bctx.rbuf;
             ((z_stream*)jcr->pZLIB_compress_workset)->avail_in  = sd->msglen;
      ((z_stream*)jcr->pZLIB_compress_workset)->next_out  = bctx.cbuf;
             ((z_stream*)jcr->pZLIB_compress_workset)->avail_out = bctx.max_compress_len;

      if ((zstat=deflate((z_stream*)jcr->pZLIB_compress_workset, Z_FINISH)) != Z_STREAM_END) {
         Jmsg(jcr, M_FATAL, 0, _("Compression deflate error: %d\n"), zstat);
         jcr->setJobStatus(JS_ErrorTerminated);
         return false;
      }
      bctx.compress_len = ((z_stream*)jcr->pZLIB_compress_workset)->total_out;
      /** reset zlib stream to be able to begin from scratch again */
      if ((zstat=deflateReset((z_stream*)jcr->pZLIB_compress_workset)) != Z_OK) {
         Jmsg(jcr, M_FATAL, 0, _("Compression deflateReset error: %d\n"), zstat);
         jcr->setJobStatus(JS_ErrorTerminated);
         return false;
      }

      Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", bctx.compress_len,
            sd->msglen);

      sd->msglen = bctx.compress_len;      /* set compressed length */
      bctx.cipher_input_len = bctx.compress_len;
   }
#endif
   return true;
}

static bool do_lzo_compression(bctx_t &bctx)
{
#ifdef HAVE_LZO
   JCR *jcr = bctx.jcr;
   BSOCK *sd = bctx.sd;
   int lzores;

   /** Do compression if turned on */
   if (bctx.ff_pkt->flags & FO_COMPRESS && bctx.ff_pkt->Compress_algo == COMPRESS_LZO1X && jcr->LZO_compress_workset) {
      lzo_uint len;          /* TODO: See with the latest patch how to handle lzo_uint with 64bit */

      ser_declare;
      ser_begin(bctx.cbuf, sizeof(comp_stream_header));

      Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", bctx.cbuf, bctx.rbuf, sd->msglen);

      lzores = lzo1x_1_compress((const unsigned char*)bctx.rbuf, sd->msglen, bctx.cbuf2,
                                &len, jcr->LZO_compress_workset);
      bctx.compress_len = len;
      if (lzores == LZO_E_OK && bctx.compress_len <= bctx.max_compress_len) {
         /* complete header */
         ser_uint32(COMPRESS_LZO1X);
         ser_uint32(bctx.compress_len);
         ser_uint16(bctx.ch.level);
         ser_uint16(bctx.ch.version);
      } else {
         /** this should NEVER happen */
         Jmsg(jcr, M_FATAL, 0, _("Compression LZO error: %d\n"), lzores);
         jcr->setJobStatus(JS_ErrorTerminated);
         return false;
      }

      Dmsg2(400, "LZO compressed len=%d uncompressed len=%d\n", bctx.compress_len,
            sd->msglen);

      bctx.compress_len += sizeof(comp_stream_header); /* add size of header */
      sd->msglen = bctx.compress_len;      /* set compressed length */
      bctx.cipher_input_len = bctx.compress_len;
   }
#endif
   return true;
}

/*
 * Do in place strip of path
 */
static bool do_strip(int count, char *in)
{
   char *out = in;
   int stripped;
   int numsep = 0;

   /** Copy to first path separator -- Win32 might have c: ... */
   while (*in && !IsPathSeparator(*in)) {
      out++; in++;
   }
   if (*in) {                    /* Not at the end of the string */
      out++; in++;
      numsep++;                  /* one separator seen */
   }
   for (stripped=0; stripped<count && *in; stripped++) {
      while (*in && !IsPathSeparator(*in)) {
         in++;                   /* skip chars */
      }
      if (*in) {
         numsep++;               /* count separators seen */
         in++;                   /* skip separator */
      }
   }
   /* Copy to end */
   while (*in) {                /* copy to end */
      if (IsPathSeparator(*in)) {
         numsep++;
      }
      *out++ = *in++;
   }
   *out = 0;
   Dmsg4(500, "stripped=%d count=%d numsep=%d sep>count=%d\n",
         stripped, count, numsep, numsep>count);
   return stripped==count && numsep>count;
}

/**
 * If requested strip leading components of the path so that we can
 *   save file as if it came from a subdirectory.  This is most useful
 *   for dealing with snapshots, by removing the snapshot directory, or
 *   in handling vendor migrations where files have been restored with
 *   a vendor product into a subdirectory.
 */
void strip_path(FF_PKT *ff_pkt)
{
   if (!(ff_pkt->flags & FO_STRIPPATH) || ff_pkt->strip_path <= 0)
   {
      Dmsg1(200, "No strip for %s\n", ff_pkt->fname);
      if (ff_pkt->snapshot_convert_fct) {
         // regress test expect this message
         Dmsg3(10, "fname=%s snap=%s link=%s\n", ff_pkt->fname, ff_pkt->snap_fname,
               ff_pkt->link);
      }
      return;
   }
   /* shared part between strip and snapshot */
   if (!ff_pkt->fname_save) {
     ff_pkt->fname_save = get_pool_memory(PM_FNAME);
     ff_pkt->link_save = get_pool_memory(PM_FNAME);
     *ff_pkt->link_save = 0;
   }
   pm_strcpy(ff_pkt->fname_save, ff_pkt->fname);
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      pm_strcpy(ff_pkt->link_save, ff_pkt->link);
      Dmsg2(500, "strcpy link_save=%d link=%d\n", strlen(ff_pkt->link_save),
         strlen(ff_pkt->link));
      Dsm_check(200);
   }

   /* See if we want also to strip the path */
   if (!(ff_pkt->flags & FO_STRIPPATH) || ff_pkt->strip_path <= 0) {
      goto rtn;
   }

   /**
    * Strip path.  If it doesn't succeed put it back.  If
    *  it does, and there is a different link string,
    *  attempt to strip the link. If it fails, back them
    *  both back.
    * Do not strip symlinks.
    * I.e. if either stripping fails don't strip anything.
    */
   if (!do_strip(ff_pkt->strip_path, ff_pkt->fname)) {
      unstrip_path(ff_pkt);
      goto rtn;
   }
   /** Strip links but not symlinks */
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      if (!do_strip(ff_pkt->strip_path, ff_pkt->link)) {
         unstrip_path(ff_pkt);
      }
   }

rtn:
   Dmsg3(10, "fname=%s stripped=%s link=%s\n", ff_pkt->fname_save, ff_pkt->fname,
       ff_pkt->link);
}

void unstrip_path(FF_PKT *ff_pkt)
{
   if (!(ff_pkt->flags & FO_STRIPPATH) || ff_pkt->strip_path <= 0)
   {
      return;
   }

   /* ff_pkt->fname can come from different places and should not be reallocated */
   if (ff_pkt->fname != ff_pkt->fname_save) {
      strcpy(ff_pkt->fname, ff_pkt->fname_save);
   }

   if (ff_pkt->link != ff_pkt->link_save) {
      if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
         strcpy(ff_pkt->link, ff_pkt->link_save);
      }
   }
}

static void close_vss_backup_session(JCR *jcr)
{
#if defined(WIN32_VSS)
   /* STOP VSS ON WIN32 */
   /* tell vss to close the backup session */
   if (jcr->Snapshot && jcr->pVSSClient) {
      if (jcr->pVSSClient->CloseBackup()) {
         /* inform user about writer states */
         for (int i=0; i<(int)jcr->pVSSClient->GetWriterCount(); i++) {
            int msg_type = M_INFO;
            if (jcr->pVSSClient->GetWriterState(i) < 1) {
               msg_type = M_WARNING;
               jcr->JobErrors++;
            }
            Jmsg(jcr, msg_type, 0, _("VSS Writer (BackupComplete): %s\n"),
                 jcr->pVSSClient->GetWriterInfo(i));
         }
      }
      /* Generate Job global writer metadata */
      WCHAR *metadata = jcr->pVSSClient->GetMetadata();
      if (metadata) {
         FF_PKT *ff_pkt = jcr->ff;
         ff_pkt->fname = (char *)"*all*"; /* for all plugins */
         ff_pkt->type = FT_RESTORE_FIRST;
         ff_pkt->LinkFI = 0;
         ff_pkt->restore_obj.object_name = (char *)"job_metadata.xml";
         ff_pkt->restore_obj.object = (char *)metadata;
         ff_pkt->restore_obj.object_len = (wcslen(metadata) + 1) * sizeof(WCHAR);
         ff_pkt->restore_obj.index = (int)time(NULL);
         save_file(jcr, ff_pkt, true);
     }
   }
#endif
}
