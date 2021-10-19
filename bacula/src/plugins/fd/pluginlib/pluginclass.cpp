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
/**
 * @file pluginclass.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon general plugin framework. The Class.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginclass.h"
#include <sys/stat.h>
#include <signal.h>
#include <sys/select.h>

/*
 * libbac uses its own sscanf implementation which is not compatible with
 * libc implementation, unfortunately.
 * use bsscanf for Bacula sscanf flavor
 */
#ifdef sscanf
#undef sscanf
#endif

// Plugin linking time variables
extern const char *PLUGINPREFIX;
extern const char *PLUGINNAME;
extern const char *PLUGINNAMESPACE;

// synchronie access to job_cancelled variable
//   smart_lock<smart_mutex> lg(&mutex); - removed on request
#define CHECK_JOB_CANCELLED \
      { \
         if (job_cancelled) { \
            return bRC_Error; \
         } \
      }

namespace pluginlib
{
   /**
    * @brief The main plugin setup method executed
    *
    * @param ctx for Bacula debug and jobinfo messages
    */
   void PLUGINBCLASS::setup_plugin(bpContext *ctx)
   {
      DMSG0(ctx, DINFO, "PLUGINCLASS::setup_plugin\n");

      getBaculaVar(bVarJobId, (void *)&JobId);
      DMSG(ctx, D1, "bVarJobId: %d\n", JobId);

      char *varpath;
      getBaculaVar(bVarExePath, (void *)&varpath);
      DMSG(ctx, DINFO, "bVarExePath: %s\n", varpath);

      pm_strcpy(execpath, varpath);
      strip_trailing_slashes(execpath.c_str());
      DMSG(ctx, DINFO, "ExePath: %s\n", execpath.c_str());

      getBaculaVar(bVarWorkingDir, (void *)&varpath);
      DMSG(ctx, DINFO, "bVarWorkingDir: %s\n", varpath);

      pm_strcpy(workingpath, varpath);
      strip_trailing_slashes(workingpath.c_str());
      DMSG(ctx, DINFO, "WorkingPath: %s\n", workingpath.c_str());
   }

   /**
    * @brief This is the main method for handling events generated by Bacula.
    *          The behavior of the method depends on event type generated, but there are
    *          some events which does nothing, just return with bRC_OK. Every event is
    *          tracked in debug trace file to verify the event flow during development.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param event a Bacula event structure
    * @param value optional event value
    * @return bRC bRC_OK - in most cases signal success/no error
    *             bRC_Error - in most cases signal error
    *             <other> - depend on Bacula Plugin API if applied
    */
   bRC PLUGINBCLASS::handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
   {
      // extract original plugin context, basically it should be `this`
      PLUGINBCLASS *pctx = (PLUGINBCLASS *)ctx->pContext;

      CHECK_JOB_CANCELLED;

      switch (event->eventType)
      {
      case bEventJobStart:
         DMSG(ctx, D3, "bEventJobStart value=%s\n", NPRT((char *)value));
         // getBaculaVar(bVarJobId, (void *)&JobId);
         getBaculaVar(bVarJobName, (void *)&JobName);
         // if (CUSTOMPREVJOBNAME){
         //    getBaculaVar(bVarPrevJobName, (void *)&prevjobname);
         // }
         return perform_jobstart(ctx);

      case bEventJobEnd:
         DMSG(ctx, D3, "bEventJobEnd value=%s\n", NPRT((char *)value));
         return perform_jobend(ctx);

      case bEventLevel:
         char lvl;
         lvl = (char)((intptr_t) value & 0xff);
         DMSG(ctx, D2, "bEventLevel='%c'\n", lvl);
         switch (lvl)
         {
         case 'F':
            DMSG0(ctx, D2, "backup level = Full\n");
            mode = BackupFull;
            break;
         case 'I':
            DMSG0(ctx, D2, "backup level = Incr\n");
            mode = BackupIncr;
            break;
         case 'D':
            DMSG0(ctx, D2, "backup level = Diff\n");
            mode = BackupDiff;
            break;
         default:
            // TODO: handle other backup levels
            DMSG0(ctx, D2, "unsupported backup level!\n");
            return bRC_Error;
         }
         return perform_joblevel(ctx, lvl);

      case bEventSince:
         since = (time_t) value;
         DMSG(ctx, D2, "bEventSince=%ld\n", (intptr_t) since);
         return perform_jobsince(ctx);

      case bEventStartBackupJob:
         DMSG(ctx, D3, "bEventStartBackupJob value=%s\n", NPRT((char *)value));
         return perform_backupjobstart(ctx);

      case bEventEndBackupJob:
         DMSG(ctx, D2, "bEventEndBackupJob value=%s\n", NPRT((char *)value));
         return perform_backupjobend(ctx);

      case bEventStartRestoreJob:
         DMSG(ctx, DINFO, "StartRestoreJob value=%s\n", NPRT((char *)value));
         getBaculaVar(bVarWhere, &where);
         DMSG(ctx, DINFO, "Where=%s\n", NPRT(where));
         getBaculaVar(bVarRegexWhere, &regexwhere);
         DMSG(ctx, DINFO, "RegexWhere=%s\n", NPRT(regexwhere));
         getBaculaVar(bVarReplace, &replace);
         DMSG(ctx, DINFO, "Replace=%c\n", replace);
         mode = Restore;
         return perform_restorejobstart(ctx);

      case bEventEndRestoreJob:
         DMSG(ctx, DINFO, "bEventEndRestoreJob value=%s\n", NPRT((char *)value));
         return perform_restorejobend(ctx);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventEstimateCommand:
         DMSG(ctx, D1, "bEventEstimateCommand value=%s\n", NPRT((char *)value));
         switch (mode)
         {
         case BackupIncr:
            mode = EstimateIncr;
            break;
         case BackupDiff:
            mode = EstimateDiff;
            break;
         default:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupFull:
            mode = EstimateFull;
            break;
         }
         pluginctx_switch_command((char *)value);
         return prepare_estimate(ctx, (char *)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventBackupCommand:
         DMSG(ctx, D2, "bEventBackupCommand value=%s\n", NPRT((char *)value));
         pluginctx_switch_command((char *)value);
         return prepare_backup(ctx, (char*)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventRestoreCommand:
         DMSG(ctx, D2, "bEventRestoreCommand value=%s\n", NPRT((char *)value));
         pluginctx_switch_command((char *)value);
         return prepare_restore(ctx, (char*)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventPluginCommand:
         DMSG(ctx, D2, "bEventPluginCommand value=%s\n", NPRT((char *)value));
         getBaculaVar(bVarAccurate, (void *)&accurate_mode);
         return prepare_command(ctx, (char*)value);

      case bEventOptionPlugin:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
      case bEventHandleBackupFile:
         if (isourplugincommand(PLUGINPREFIX, (char*)value)){
            DMSG0(ctx, DERROR, "Invalid handle Option Plugin called!\n");
            JMSG2(ctx, M_FATAL,
                  "The %s plugin doesn't support the Option Plugin configuration.\n"
                  "Please review your FileSet and move the Plugin=%s"
                  "... command into the Include {} block.\n",
                  PLUGINNAME, PLUGINPREFIX);
            return bRC_Error;
         }
         break;

      case bEventEndFileSet:
         DMSG(ctx, D3, "bEventEndFileSet value=%s\n", NPRT((char *)value));
         return perform_jobendfileset(ctx);

      case bEventRestoreObject:
         /* Restore Object handle - a plugin configuration for restore and user supplied parameters */
         if (!value){
            DMSG0(ctx, DINFO, "End restore objects\n");
            break;
         }
         DMSG(ctx, D2, "bEventRestoreObject value=%p\n", value);
         return parse_plugin_restore_object(ctx, (restore_object_pkt *) value);

      case bEventCancelCommand:
         DMSG2(ctx, D3, "bEventCancelCommand self = %p pctx = %p\n", this, pctx);
         pctx->job_cancelled = true;
         return pctx->perform_cancel_command(ctx);

      default:
         // enabled only for Debug
         DMSG2(ctx, D2, "Unknown event: %s (%d) \n", eventtype2str(event), event->eventType);
      }

      return bRC_OK;
   }

   /**
    * @brief Parsing a plugin command.
    *
    * @param ctx bpContext - Bacula Plugin context structure
    * @param command plugin command string to parse
    * @param params output parsed params list
    * @return bRC bRC_OK - on success, bRC_Error - on error
    */
   bRC PLUGINBCLASS::parse_plugin_command(bpContext *ctx, const char *command)
   {
      DMSG(ctx, DINFO, "Parse command: %s\n", command);
      if (parser.parse_cmd(command) != bRC_OK) {
         DMSG0(ctx, DERROR, "Unable to parse Plugin command line.\n");
         JMSG0(ctx, M_FATAL, "Unable to parse Plugin command line.\n");
         return bRC_Error;
      }

      /* switch pluginctx to the required context or allocate a new one */
      pluginctx_switch_command(command);

      /* the first (zero) parameter is a plugin name, we should skip it */
      for (int i = 1; i < parser.argc; i++)
      {
         /* scan for abort_on_error parameter */
         if (strcasecmp(parser.argk[i], "abort_on_error") == 0){
            /* found, so check the value if provided, I only check the first char */
            if (parser.argv[i] && *parser.argv[i] == '0') {
               pluginctx_clear_abort_on_error();
            } else {
               pluginctx_set_abort_on_error();
            }
            DMSG1(ctx, DINFO, "abort_on_error found: %s\n", pluginctx_is_abort_on_error() ? "True" : "False");
            continue;
         }
         /* scan for listing parameter, so the estimate job should be executed as a Listing procedure */
         if (EstimateFull == mode && bstrcmp(parser.argk[i], "listing")){
            /* we have a listing parameter which for estimate means .ls command */
            mode = Listing;
            DMSG0(ctx, DINFO, "listing procedure param found\n");
            continue;
         }
         /* scan for query parameter, so the estimate job should be executed as a QueryParam procedure */
         if (EstimateFull == mode && strcasecmp(parser.argk[i], "query") == 0){
            /* found, so check the value if provided */
            if (parser.argv[i]){
               mode = QueryParams;
               DMSG0(ctx, DINFO, "query procedure param found\n");
            }
         }
         /* handle it with pluginctx */
         bRC status = pluginctx_parse_parameter(ctx, parser.argk[i], parser.argv[i]);
         switch (status)
         {
         case bRC_OK:
            /* the parameter was handled by xenctx, proceed to the next */
            continue;
         case bRC_Error:
            /* parsing returned error, raise it up */
            return bRC_Error;
         default:
            break;
         }

         DMSG(ctx, DERROR, "Unknown parameter: %s\n", parser.argk[i]);
         JMSG(ctx, pluginctx_jmsg_err_level(), "Unknown parameter: %s\n", parser.argk[i]);
      }

#if 0
      /* count the numbers of user parameters if any */
      // count = get_ini_count();

      /* the first (zero) parameter is a plugin name, we should skip it */
      argc = parser.argc - 1;
      parargc = argc + count;
      /* first parameters from plugin command saved during backup */
      for (int i = 1; i < parser.argc; i++) {
         param = new POOL_MEM(PM_FNAME);     // TODO: change to POOL_MEM
         found = false;

         int k;
         /* check if parameter overloaded by restore parameter */
         if ((k = check_ini_param(parser.argk[i])) != -1){
            found = true;
            DMSG1(ctx, DINFO, "parse_plugin_command: %s found in restore parameters\n", parser.argk[i]);
            if (render_param(ctx, *param, ini.items[k].handler, parser.argk[i], ini.items[k].val) != bRC_OK){
               delete(param);
               return bRC_Error;
            }
            params.append(param);
            parargc--;
         }

         /* check if param overloaded above */
         if (!found){
            if (parser.argv[i]){
               Mmsg(*param, "%s=%s\n", parser.argk[i], parser.argv[i]);
               params.append(param);
            } else {
               Mmsg(*param, "%s=1\n", parser.argk[i]);
               params.append(param);
            }
         }
         /* param is always ended with '\n' */
         DMSG(ctx, DINFO, "Param: %s", param);

      }
      /* check what was missing in plugin command but get from ini file */
      if (argc < parargc){
         for (int k = 0; ini.items[k].name; k++){
            if (ini.items[k].found && !check_plugin_param(ini.items[k].name, &params)){
               param = new POOL_MEM(PM_FNAME);
               DMSG1(ctx, DINFO, "parse_plugin_command: %s from restore parameters\n", ini.items[k].name);
               if (render_param(ctx, *param, ini.items[k].handler, (char*)ini.items[k].name, ini.items[k].val) != bRC_OK){
                  delete(param);
                  return bRC_Error;
               }
               params.append(param);
               /* param is always ended with '\n' */
               DMSG(ctx, DINFO, "Param: %s", param);
            }
         }
      }
#endif
      return bRC_OK;
   }

   /**
    * @brief Parse a Restore Object saved during backup. It handle both plugin config and other restore objects.
    *
    * @param ctx Bacula Plugin context structure
    * @param rop a restore object structure to parse
    * @return bRC bRC_OK - on success
    *             bRC_Error - on error
    */
   bRC PLUGINBCLASS::parse_plugin_restore_object(bpContext *ctx, restore_object_pkt *rop)
   {
      if (!rop){
         return bRC_OK;    /* end of rop list */
      }

      DMSG2(ctx, DDEBUG, "parse_plugin_restore_object: %s %d\n", rop->object_name, rop->object_type);

      pluginctx_switch_command(rop->plugin_name);

      // first check plugin config
      if (strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME) == 0 && (rop->object_type == FT_PLUGIN_CONFIG || rop->object_type == FT_PLUGIN_CONFIG_FILLED)) {
         /* we have a single config RO for every command */
         DMSG(ctx, DINFO, "plugin config for: %s\n", rop->plugin_name);
         bRC status = parse_plugin_config(ctx, rop);
         if (status != bRC_OK) {
            return bRC_Error;
         }
         return pluginctx_parse_plugin_config(ctx, rop);
      }

      // handle any other RO restore
      bRC status = handle_plugin_restore_object(ctx, rop);
      if (status != bRC_OK) {
         return bRC_Error;
      }

      return pluginctx_handle_restore_object(ctx, rop);
   }

   /**
    * @brief Handle Bacula Plugin I/O API
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param io Bacula Plugin API I/O structure for I/O operations
    * @return bRC bRC_OK - when successful
    *             bRC_Error - on any error
    *             io->status, io->io_errno - correspond to a plugin io operation status
    */
   bRC PLUGINBCLASS::pluginIO(bpContext *ctx, struct io_pkt *io)
   {
      static int rw = 0;      // this variable handles single debug message

      CHECK_JOB_CANCELLED;

      /* assume no error from the very beginning */
      io->status = 0;
      io->io_errno = 0;
      switch (io->func)
      {
      case IO_OPEN:
         DMSG(ctx, D2, "IO_OPEN: (%s)\n", io->fname);
         switch (mode)
         {
         case BackupFull:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupDiff:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupIncr:
            return perform_backup_open(ctx, io);

         case Restore:
            return perform_restore_open(ctx, io);

         default:
            return bRC_Error;
         }
         break;

      case IO_READ:
         if (!rw) {
            rw = 1;
            DMSG2(ctx, D2, "IO_READ buf=%p len=%d\n", io->buf, io->count);
         }
         switch (mode)
         {
         case BackupFull:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupDiff:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupIncr:
            return perform_read_data(ctx, io);

         default:
            return bRC_Error;
         }
         break;
      case IO_WRITE:
         if (!rw) {
            rw = 1;
            DMSG2(ctx, D2, "IO_WRITE buf=%p len=%d\n", io->buf, io->count);
         }
         switch (mode)
         {
         case Restore:
            return perform_write_data(ctx, io);

         default:
            return bRC_Error;
         }
         break;

      case IO_CLOSE:
         DMSG0(ctx, D2, "IO_CLOSE\n");
         rw = 0;
         switch (mode)
         {
         case Restore:
            return perform_restore_close(ctx, io);

         case BackupFull:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupDiff:
#if __cplusplus >= 201703L
            [[fallthrough]];
#endif
         case BackupIncr:
            return perform_backup_close(ctx, io);
         default:
            return bRC_Error;
         }
         break;

      case IO_SEEK:
         DMSG2(ctx, D2, "IO_SEEK off=%lld wh=%d\n", io->offset, io->whence);
         switch (mode)
         {
         case Restore:
            return perform_seek_write(ctx, io);
         default:
            return bRC_Error;
         }
         break;
      }

      return bRC_OK;
   }

   /**
    * @brief Get all required information from backend to populate save_pkt for Bacula.
    *    It handles a Restore Object (FT_PLUGIN_CONFIG) for every Full backup and
    *    new Plugin Backup Command if setup in FileSet. It uses a help from
    *    endBackupFile() handling the next FNAME command for the next file to
    *    backup. The communication protocol requires some file attributes command
    *    required it raise the error when insufficient parameters received from
    *    backend. It assumes some parameters at save_pkt struct to be automatically
    *    set like: sp->portable, sp->statp.st_blksize, sp->statp.st_blocks.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param sp Bacula Plugin API save packet structure
    * @return bRC bRC_OK - when save_pkt prepared successfully and we have file to backup
    *             bRC_Max - when no more files to backup
    *             bRC_Error - in any error
    */
   bRC PLUGINBCLASS::startBackupFile(bpContext *ctx, struct save_pkt *sp)
   {
      CHECK_JOB_CANCELLED;

      /* The first file in Full backup, is the Plugin Config */
      if (mode == BackupFull && pluginconfigsent == false) {
         ConfigFile ini;
         ini.register_items(plugin_items_dump, sizeof(struct ini_items));
         sp->restore_obj.object_name = (char *)INI_RESTORE_OBJECT_NAME;
         sp->restore_obj.object_len = ini.serialize(robjbuf.handle());
         sp->restore_obj.object = robjbuf.c_str();
         sp->type = FT_PLUGIN_CONFIG;
         DMSG2(ctx, DINFO, "Prepared RestoreObject/%s (%d) sent.\n", INI_RESTORE_OBJECT_NAME, FT_PLUGIN_CONFIG);
         return bRC_OK;
      }

      bRC status = perform_start_backup_file(ctx, sp);

      DMSG2(ctx, DINFO, "StartBackup: %s %ld\n", sp->fname, sp->statp.st_size);

      // DMSG3(ctx, DINFO, "TSDebug: %ld(at) %ld(mt) %ld(ct)\n",
      //       sp->statp.st_atime, sp->statp.st_mtime, sp->statp.st_ctime);

      return status;
   }

   /**
    * @brief Check for a next file to backup or the end of the backup loop.
    *    The next file to backup is indicated by a FNAME command from backend and
    *    no more files to backup as EOD. It helps startBackupFile handling FNAME
    *    for next file.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @return bRC bRC_OK - when no more files to backup
    *             bRC_More - when Bacula should expect a next file
    *             bRC_Error - in any error
    */
   bRC PLUGINBCLASS::endBackupFile(bpContext *ctx)
   {
      CHECK_JOB_CANCELLED;

      /* When current file was the Plugin Config, so just ask for the next file */
      if (mode == BackupFull && pluginconfigsent == false) {
         pluginconfigsent = true;
         return bRC_More;
      }

      bRC status = perform_end_backup_file(ctx);
      if (status == bRC_More) {
         DMSG1(ctx, DINFO, "Nextfile %s backup!\n", fname.c_str());
      }

      return status;
   }

   /**
    * @brief
    *
    * @param ctx
    * @param cmd
    * @return bRC
    */
   bRC PLUGINBCLASS::startRestoreFile(bpContext *ctx, const char *cmd)
   {
      CHECK_JOB_CANCELLED;

      return perform_start_restore_file(ctx, cmd);
   }

   /**
    * @brief
    *
    * @param ctx
    * @return bRC
    */
   bRC PLUGINBCLASS::endRestoreFile(bpContext *ctx)
   {
      CHECK_JOB_CANCELLED;

      return perform_end_restore_file(ctx);
   }

   /**
    * @brief Prepares a file to restore attributes based on data from restore_pkt.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param rp Bacula Plugin API restore packet structure
    * @return bRC bRC_OK - when success
    *             rp->create_status = CF_EXTRACT - the plugin will restore the file itself
    *             rp->create_status = CF_SKIP - the plugin wants to skip restoration, i.e. the file already exist and Replace=n was set
    *             rp->create_status = CF_CORE - the plugin wants Bacula to do the restore
    *             bRC_Error, rp->create_status = CF_ERROR - in any error
    */
   bRC PLUGINBCLASS::createFile(bpContext *ctx, struct restore_pkt *rp)
   {
      CHECK_JOB_CANCELLED;

      if (CORELOCALRESTORE && islocalpath(where)) {
         DMSG0(ctx, DDEBUG, "createFile:Forwarding restore to Core\n");
         rp->create_status = CF_CORE;
         return bRC_OK;
      }

      return perform_restore_create_file(ctx, rp);
   }

   /**
    * @brief
    *
    * @param ctx
    * @param rp
    * @return bRC
    */
   bRC PLUGINBCLASS::setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
   {
      CHECK_JOB_CANCELLED;

      return perform_restore_set_file_attributes(ctx, rp);
   }

   /**
    * @brief Implements default pluginclass checkFile() callback.
    *    When fname match plugin configured namespace then it return bRC_Seen by default
    *    if no custom perform_backup_check_file() method is implemented by developer.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param fname file name to check
    * @return bRC bRC_Seen or bRC_OK
    */
   bRC PLUGINBCLASS::checkFile(bpContext * ctx, char *fname)
   {
      CHECK_JOB_CANCELLED;

      DMSG2(ctx, DINFO, "PLUGINBCLASS::checkFile: %s %s\n", PLUGINNAMESPACE, fname);
      if (isourpluginfname(PLUGINNAMESPACE, fname))
      {
         return perform_backup_check_file(ctx, fname);
      }

      return bRC_OK;
   }

   /**
    * @brief Handle ACL and XATTR data during backup or restore.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param xacl
    * @return bRC bRC_OK when successfull, OK
    *             bRC_Error on any error found
    */
   bRC PLUGINBCLASS::handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
   {
      CHECK_JOB_CANCELLED;

      // defaults
      xacl->count = 0;
      xacl->content = NULL;

      switch (xacl->func)
      {
      case BACL_BACKUP:
         return perform_acl_backup(ctx, xacl);
      case BACL_RESTORE:
         return perform_acl_restore(ctx, xacl);
      case BXATTR_BACKUP:
         return perform_xattr_backup(ctx, xacl);
      case BXATTR_RESTORE:
         return perform_xattr_restore(ctx, xacl);
      default:
         DMSG1(ctx, DERROR, "PLUGINBCLASS::handleXACLdata: unknown xacl function: %d\n", xacl->func);
      }

      return bRC_OK;
   }

   /**
    * @brief
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param qp
    * @return bRC bRC_OK when successfull, OK
    *             bRC_Error on any error found
    */
   bRC PLUGINBCLASS::queryParameter(bpContext *ctx, struct query_pkt *qp)
   {
      // check if it is our Plugin command
      if (!isourplugincommand(PLUGINPREFIX, qp->command) != 0){
         // it is not our plugin prefix
         return bRC_OK;
      }

      CHECK_JOB_CANCELLED;

      if (mode != QueryParams) {
         mode = QueryParams;
      }

      return perform_query_parameter(ctx, qp);
   }

   /**
    * @brief
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param mp
    * @return bRC bRC_OK when successfull, OK
    *             bRC_Error on any error found
    */
   bRC PLUGINBCLASS::metadataRestore(bpContext *ctx, struct meta_pkt *mp)
   {
      CHECK_JOB_CANCELLED;

      return perform_restore_metadata(ctx, mp);
   }

}  // namespace pluginlib
