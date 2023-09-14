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
 * A simple pipe plugin for the Bacula File Daemon
 *
 *  Kern Sibbald, October 2007
 *
 */
#include "bacula.h"
#define USE_FULL_WRITE
#include "fd_common.h"
#include "fd_plugins.h"
#include "lib/ini.h"
#include "fileopts.h"

#undef malloc
#undef free
#undef strdup

#define fi __FILE__
#define li __LINE__

#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "January 2008"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Bacula Pipe File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC checkFile(bpContext *ctx, char *fname);
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);

static char *apply_rp_codes(struct plugin_ctx * p_ctx);
static bool dump_restoreobject(bpContext *ctx, restore_object_pkt *rop);

/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/* Plugin Information block */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION
};

/* Plugin entry points for Bacula */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes,
   checkFile,
   handleXACLdata,
   NULL                               /* No checkStream */
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   boffset_t offset;
   BPIPE *pfd;                        /* bpipe file descriptor */
   int  efd;                          /* stderr */
   int  rfd;                          /* stdout */
   int  wfd;                          /* stdin */
   int  maxfd;                        /* max(stderr, stdout) */
   bool backup;                       /* set when the backup is done */
   bool restore_object_sent;
   bool canceled;
   char *cmd;                         /* plugin command line */
   char *fname;                       /* filename to "backup/restore" */
   char *reader;                      /* reader program for backup */
   char *writer;                      /* writer program for backup */
   alist *rop_writer;                 /* writer command set via restore object */
   char where[512];
   int replace;
   int job_level;
   int estimate_mode;
   int64_t total_bytes;         /* number of bytes read/write */
   POOLMEM *restore_obj_buf;
};

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 */
/*
 * External entry point called by Bacula to "load the plugin
 */
bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * External entry point to unload the plugin 
 */
bRC unloadPlugin() 
{
// printf("bpipe-fd: Unloaded\n");
   return bRC_OK;
}

class restoreobj: public SMARTALLOC {
public:
   char *name;
   char *writer;
   restoreobj(char *n, char *w) {
      name = bstrdup(n);
      writer = bstrdup(w);
   };
   ~restoreobj() {
      bfree(writer);
      bfree(name);
   };
};


/*
 * The following entry points are accessed through the function 
 *   pointers we supplied to Bacula. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
/*
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */
   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   bfree_and_null(p_ctx->cmd);
   free_and_null_pool_memory(p_ctx->restore_obj_buf);
   if (p_ctx->rop_writer) {
      restoreobj *rop;
      foreach_alist(rop, p_ctx->rop_writer) {
         delete rop;
      }
      delete p_ctx->rop_writer;
   }
   free(p_ctx);                          /* free our private context */
   p_ctx = NULL;
   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Handle an event that was generated in Bacula
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   restore_object_pkt *rop;

   if (!p_ctx) {
      return bRC_Error;
   }

// char *name;

   /*
    * Most events don't interest us so we ignore them.
    *   the printfs are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {
   case bEventLevel:
      p_ctx->job_level = ((intptr_t)value);
      break;

   case bEventCancelCommand:
      p_ctx->canceled = true;
      break;

   case bEventPluginCommand:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
                           "bpipe-fd: PluginCommand=%s\n", (char *)value);
      break;
   case bEventJobStart:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    printf("bpipe-fd: JobEnd\n");
      break;
   case bEventStartBackupJob:
//    printf("bpipe-fd: StartBackupJob\n");
      break;
   case bEventEndBackupJob:
//    printf("bpipe-fd: EndBackupJob\n");
      break;
   case bEventSince:
//    printf("bpipe-fd: since=%d\n", (int)value);
      break;
   case bEventRestoreObject:
      if (!value) {
         break;
      }
      rop = (restore_object_pkt *)value;
      if (!dump_restoreobject(ctx, rop)) {
         return bRC_Error;
      }
      break;

   case bEventStartRestoreJob:
//    printf("bpipe-fd: StartRestoreJob\n");
      break;

   case bEventEndRestoreJob:
//    printf("bpipe-fd: EndRestoreJob\n");
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventEstimateCommand:
      p_ctx->estimate_mode = true;
      /* Fall-through wanted */
   case bEventRestoreCommand:
//    printf("bpipe-fd: EventRestoreCommand cmd=%s\n", (char *)value);
      /* Fall-through wanted */
   case bEventBackupCommand:
      char *p;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: pluginEvent cmd=%s\n", (char *)value);
      p_ctx->restore_object_sent = false; /* Foreach command, we might have to send a specific restore object */
      p_ctx->backup = false;
      p_ctx->cmd = strdup((char *)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Plugin terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "File terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Reader terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate reader string */
      p_ctx->writer = p;

      /* We may already have a writer command set via restore object */
      if (p_ctx->rop_writer) {
         restoreobj *rop;
         foreach_alist(rop, p_ctx->rop_writer) {
            if (!strcmp(rop->name, (char *)value)) {
               p_ctx->writer = rop->writer;
            }
         }
      }
//    printf("bpipe-fd: plugin=%s fname=%s reader=%s writer=%s\n", 
//         p_ctx->cmd, p_ctx->fname, p_ctx->reader, p_ctx->writer);
      break;

   default:
//    printf("bpipe-fd: unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

/* Configuration parameters for restore  */
static struct ini_items my_items[] = {
 // name                        handler         comment                 required   default
 { "restore_command",          ini_store_str,  "Restore command to use",    0,    NULL},
 { NULL,       NULL,           NULL,                                         0,    NULL}
};

static bool dump_restoreobject(bpContext *ctx, restore_object_pkt *rop)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return false;
   }

   bfuncs->DebugMessage(ctx, fi, li, dbglvl, "Trying to dump restore object\n");

   if (!strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME)) {
      ConfigFile ini;

      if (!ini.dump_string(rop->object, rop->object_len)) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0,
                            "Unable to parse the User supplied restore options\n");
         bfuncs->DebugMessage(ctx, fi, li, 0, "Can't parse configuration file\n");
         return false;
      }

      ini.register_items(my_items, sizeof(struct ini_items)); 

      if (ini.parse(ini.out_fname)) {
         /* TODO: check parameters */
         if (ini.items[0].found) {
            if (!p_ctx->rop_writer) {
               p_ctx->rop_writer = New(alist(5, not_owned_by_alist));
            }
            p_ctx->rop_writer->append(New(restoreobj(rop->plugin_name, ini.items[0].val.strval)));
            bfuncs->JobMessage(ctx, fi, li, M_INFO, 0,
                               _("Using user supplied restore command: \"%s\"\n"), ini.items[0].val.strval);
         } else {
            bfuncs->DebugMessage(ctx, fi, li, 0, "Options not set\n");
         }
      } else {
         bfuncs->DebugMessage(ctx, fi, li, 0, "Can't parse configuration file\n");
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0,
                            "User supplied restore options are not valid\n");
         return false;
      }
   }
   return true;
}

/* 
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

   /* First step is to send config restoreobject if needed */
   if (!p_ctx->restore_object_sent && p_ctx->job_level == 'F' && !p_ctx->estimate_mode)
   {
      ConfigFile ini;
      POOLMEM *buf = get_pool_memory(PM_MESSAGE);
      p_ctx->restore_object_sent = true;
      ini.register_items(my_items, sizeof(struct ini_items));
      sp->restore_obj.object_name = (char*)INI_RESTORE_OBJECT_NAME;
      sp->restore_obj.object_len = ini.serialize(&buf);
      sp->restore_obj.object = buf;
      sp->type = FT_PLUGIN_CONFIG;
      p_ctx->restore_obj_buf = buf;
      return bRC_OK;
   }

   time_t now = time(NULL);
   sp->fname = p_ctx->fname;
   sp->type = FT_REG;
   sp->statp.st_mode = 0700 | S_IFREG;
   sp->statp.st_ctime = now;
   sp->statp.st_mtime = now;
   sp->statp.st_atime = now;
   sp->statp.st_size = -1;
   sp->statp.st_blksize = 4096;
   sp->statp.st_blocks = 1;
#ifdef TEST_BPIPE_OFFSET
   sp->flags |= FO_OFFSETS;
#endif
   p_ctx->backup = true;
   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * We would return bRC_More if we wanted startBackupFile to be
    * called again to backup another file
    */
   if (!p_ctx->backup) {  /* We sent the RestoreObject, we need to send the data */
      return bRC_More;
   }
   return bRC_OK;
}

static void send_log(bpContext *ctx, char *buf)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   strip_trailing_newline(buf);
   bfuncs->JobMessage(ctx, fi, li, M_INFO, 0, "%s: %s\n", p_ctx->fname, buf);
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   fd_set rfds;
   fd_set wfds;
   bool ok=false;
   char buf[1024];
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

   io->status = -1;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      p_ctx->total_bytes = 0;
      p_ctx->wfd = p_ctx->efd = p_ctx->rfd = -1;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN\n");
      if (io->flags & (O_CREAT | O_WRONLY)) {
         char *writer_codes = apply_rp_codes(p_ctx);

         p_ctx->pfd = open_bpipe(writer_codes, 0, "rws");
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN fd=%p writer=%s\n", 
             p_ctx->pfd, writer_codes);
         if (!p_ctx->pfd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
               "Open pipe writer=%s failed: ERR=%s\n", writer_codes, strerror(errno));
            if (writer_codes) {
               free(writer_codes);
            }
            return bRC_Error;
         }
         if (writer_codes) {
            free(writer_codes);
         }
         /* We need to read from stdout/stderr for messages to display to the user */
         p_ctx->rfd = fileno(p_ctx->pfd->rfd);
         p_ctx->wfd = fileno(p_ctx->pfd->wfd);
         p_ctx->maxfd = MAX(p_ctx->wfd, p_ctx->rfd);
         io->status = p_ctx->wfd;

      } else {
         /* Use shell mode and split stderr/stdout */
         p_ctx->pfd = open_bpipe(p_ctx->reader, 0, "rse");
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN fd=%p reader=%s\n", 
            p_ctx->pfd, p_ctx->reader);
         if (!p_ctx->pfd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
               "Open pipe reader=%s failed: ERR=%s\n", p_ctx->reader, strerror(errno));
            return bRC_Error;
         }
         /* We need to read from stderr for job log and stdout for the data */
         p_ctx->efd = fileno(p_ctx->pfd->efd);
         p_ctx->rfd = fileno(p_ctx->pfd->rfd);
         p_ctx->maxfd = MAX(p_ctx->efd, p_ctx->rfd);
         io->status = p_ctx->rfd;
      }
      sleep(1);                 /* let pipe connect */
      break;

   case IO_READ:
      if (!p_ctx->pfd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL read FD\n");
         return bRC_Error;
      }

      /* We first try to read stderr, but keep monitoring for data on stdout (when stderr is empty) */
      while (!p_ctx->canceled) {
         FD_ZERO(&rfds);
         FD_SET(p_ctx->rfd, &rfds);
         FD_SET(p_ctx->efd, &rfds); 
         select(p_ctx->maxfd+1, &rfds, NULL, NULL, NULL);

         if (!FD_ISSET(p_ctx->efd, &rfds)) {
            /* nothing in stderr, then we should have something in stdout */
            break;
         }
         int ret = read(p_ctx->efd, buf, sizeof(buf));
         if (ret <= 0) {
            /* stderr is closed or in error, stdout should be in the same state */
            /* let handle it at the stdout level */
            break;
         }
         /* TODO: buffer and split lines */
         buf[ret]=0;
         send_log(ctx, buf);
      }

      io->status = read(p_ctx->rfd, io->buf, io->count);
//    bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_READ buf=%p len=%d\n", io->buf, io->status);
      if (io->status < 0) {
         berrno be;
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
                            "Pipe read error: ERR=%s\n", be.bstrerror());
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
                              "Pipe read error: count=%lld errno=%d ERR=%s\n",
                              p_ctx->total_bytes, (int)errno, be.bstrerror());
         return bRC_Error;
      }
      io->offset = p_ctx->total_bytes;
      p_ctx->total_bytes += io->status;
      break;

   case IO_WRITE:
      if (!p_ctx->pfd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL write FD\n");
         return bRC_Error;
      }

      /* When we write, we must check for the error channel (stdout+stderr) as well */
      while (!ok && !p_ctx->canceled) {
         FD_ZERO(&wfds);
         FD_SET(p_ctx->wfd, &wfds);
         FD_ZERO(&rfds);
         FD_SET(p_ctx->rfd, &rfds);

         select(p_ctx->maxfd+1, &rfds, &wfds, NULL, NULL);

         if (FD_ISSET(p_ctx->rfd, &rfds)) {
            int ret = read(p_ctx->rfd, buf, sizeof(buf)); /* TODO: simulate fgets() */
            if (ret > 0) {
               buf[ret]=0;
               send_log(ctx, buf); 
            } else {
               ok = true;       /* nothing to read */
            }
         }

         if (FD_ISSET(p_ctx->wfd, &wfds)) {
            ok = true;
         }
      }

//    printf("bpipe-fd: IO_WRITE fd=%p buf=%p len=%d\n", p_ctx->fd, io->buf, io->count);
      io->status = full_write(p_ctx->wfd, io->buf, io->count, &p_ctx->canceled);
//    printf("bpipe-fd: IO_WRITE buf=%p len=%d\n", io->buf, io->status);
      if (io->status <= 0) {
         berrno be;
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
                            "Pipe write error: ERR=%s\n", be.bstrerror());
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
                              "Pipe write error: count=%lld errno=%d ERR=%s\n",
                              p_ctx->total_bytes, (int)errno, be.bstrerror());
         return bRC_Error;
      }
      p_ctx->total_bytes += io->status;
      break;

   case IO_CLOSE:
      if (!p_ctx->pfd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL FD on bpipe close\n");
         return bRC_Error;
      }

      /* We inform the other side that we have nothing more to send */
      if (p_ctx->wfd >= 0) {
         int ret = close_wpipe(p_ctx->pfd);
         if (ret == 0) {
            bfuncs->JobMessage(ctx, fi, li, M_ERROR, 0, "bpipe-fd: Error closing for file %s: %d\n", 
                               p_ctx->fname, ret);
         }
      }

      /* We flush what the other program has to say */
      while (!ok && !p_ctx->canceled) {
         struct timeval tv = {10, 0};   // sleep for 10secs
         FD_ZERO(&rfds);
         p_ctx->maxfd = -1;

         if (p_ctx->rfd >= 0) {
            FD_SET(p_ctx->rfd, &rfds);
            p_ctx->maxfd = MAX(p_ctx->maxfd, p_ctx->rfd);
         }

         if (p_ctx->efd >= 0) {
            FD_SET(p_ctx->efd, &rfds);
            p_ctx->maxfd = MAX(p_ctx->maxfd, p_ctx->efd);
         }

         if (p_ctx->maxfd == -1) {
            ok = true;          /* exit the loop */
         } else {
            select(p_ctx->maxfd+1, &rfds, NULL, NULL, &tv);
         }

         if (p_ctx->rfd >= 0 && FD_ISSET(p_ctx->rfd, &rfds)) {
            int ret = read(p_ctx->rfd, buf, sizeof(buf));
            if (ret > 0) {
               buf[ret]=0;
               send_log(ctx, buf);
            } else {
               p_ctx->rfd = -1; /* closed, keep the reference in bpipe */
            }
         }

         /* The stderr can be melted with stdout or not */
         if (p_ctx->efd >= 0 && FD_ISSET(p_ctx->efd, &rfds)) {
            int ret = read(p_ctx->efd, buf, sizeof(buf));
            if (ret > 0) {
               buf[ret]=0;
               send_log(ctx, buf);
            } else {
               p_ctx->efd = -1; /* closed, keep the reference in bpipe */
            }
         }
      }

      io->status = close_bpipe(p_ctx->pfd);
      if (io->status != 0) {
         bfuncs->JobMessage(ctx, fi, li, M_ERROR, 0, "bpipe-fd: Error closing for file %s: %d\n",
                            p_ctx->fname, io->status);
      }
      break;

   case IO_SEEK:
      io->offset = p_ctx->offset;
      io->status = 0;
      break;
   }
   return bRC_OK;
}

/*
 * Bacula is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
// printf("bpipe-fd: startRestoreFile cmd=%s\n", cmd);
   return bRC_OK;
}

/*
 * Bacula is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
// printf("bpipe-fd: endRestoreFile\n");
   return bRC_OK;
}

/*
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *   
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: createFile\n");
   uint sz = sizeof(((struct plugin_ctx *)ctx->pContext)->where);
   if (strlen(rp->where) > sz) {
      printf("Restore target dir too long. Restricting to first %u bytes.\n", sz);
   }
   bstrncpy(((struct plugin_ctx *)ctx->pContext)->where, rp->where, sz);
   ((struct plugin_ctx *)ctx->pContext)->replace = rp->replace;
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: setFileAttributes\n");
   return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(bpContext *ctx, char *fname)
{
   return bRC_OK;
}

/*
 * New Bacula Plugin API require this
 */
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   return bRC_OK;
}

/*************************************************************************
 * Apply codes in writer command:
 * %w -> "where"
 * %r -> "replace"
 *
 * Replace:
 * 'always' => 'a', chr(97)
 * 'ifnewer' => 'w', chr(119)
 * 'ifolder' => 'o', chr(111)
 * 'never' => 'n', chr(110)
 *
 * This function will allocate the required amount of memory with malloc.
 * Need to be free()d manually.
 * Inspired by edit_job_codes in lib/util.c
 */

static char *apply_rp_codes(struct plugin_ctx * p_ctx)
{
   char *p, *q;
   const char *str;
   char add[10];
   int w_count = 0, r_count = 0;
   char *omsg;

   char *imsg = p_ctx->writer;

   if (!imsg) {
      return NULL;
   }

   if ((p = imsg)) {
      while ((q = strstr(p, "%w"))) {
         w_count++;
         p=q+1;
      }

      p = imsg;
      while ((q = strstr(p, "%r"))) {
         r_count++;
         p=q+1;
      }
   }

   /* Required mem: 
    * len(imsg) 
    * + number of "where" codes * (len(where)-2) 
    * - number of "replace" codes
    */
   omsg = (char*)malloc(strlen(imsg) + (w_count * (strlen(p_ctx->where)-2)) - r_count + 1);
   if (!omsg) {
      fprintf(stderr, "Out of memory.");
      return NULL;
   }

   *omsg = 0;
   //printf("apply_rp_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'w':
             str = p_ctx->where;
             break;
         case 'r':
            snprintf(add, 2, "%c", p_ctx->replace);
            str = add;
            break;
         default:
            add[0] = '%';
            add[1] = *p;
            add[2] = 0;
            str = add;
            break;
         }
      } else {
         add[0] = *p;
         add[1] = 0;
         str = add;
      }
      //printf("add_str %s\n", str);
      strcat(omsg, str);
      //printf("omsg=%s\n", omsg);
   }
   return omsg;
}

#ifdef __cplusplus
}
#endif
