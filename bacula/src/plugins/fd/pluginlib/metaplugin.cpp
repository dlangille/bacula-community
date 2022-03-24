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
/**
 * @file metaplugin.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula metaplugin interface.
 * @version 2.1.0
 * @date 2020-12-23
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "metaplugin.h"
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
#define NEED_REVIEW

#define pluginclass(ctx)     (METAPLUGIN*)ctx->pContext;

// Job Info Types
#define  BACKEND_JOB_INFO_BACKUP       'B'
#define  BACKEND_JOB_INFO_ESTIMATE     'E'
#define  BACKEND_JOB_INFO_RESTORE      'R'

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
static bRC metaplugincheckFile(bpContext *ctx, char *fname);
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp);
static bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp);

/* Pointers to Bacula functions */
bFuncs *bfuncs = NULL;
bInfo *binfo = NULL;

static pFuncs pluginFuncs =
{
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,
   freePlugin,
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
   metaplugincheckFile,
   handleXACLdata,
   NULL,                        /* No restore file list */
   NULL,                        /* No checkStream */
   queryParameter,
   metadataRestore,
};

#ifdef __cplusplus
extern "C" {
#endif

/* Plugin Information structure */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

/*
 * Plugin called here when it is first loaded
 */
bRC DLL_IMP_EXP loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo ** pinfo, pFuncs ** pfuncs)
{
   bfuncs = lbfuncs;               /* set Bacula function pointers */
   binfo = lbinfo;

   Dmsg3(DINFO, "%s Plugin version %s %s (c) 2020 by Inteos\n", PLUGINNAME, PLUGIN_VERSION, PLUGIN_DATE);

   *pinfo = &pluginInfo;           /* return pointer to our info */
   *pfuncs = &pluginFuncs;         /* return pointer to our functions */

   return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when Bacula is going to exit.
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/*
 * Main PLUGIN class constructor.
 */
METAPLUGIN::METAPLUGIN(bpContext *bpctx) :
      backend_cmd(PM_FNAME),
      ctx(NULL),
      backend_available(false),
      backend_error(PM_MESSAGE),
      mode(NONE),
      JobId(0),
      JobName(NULL),
      since(0),
      where(NULL),
      regexwhere(NULL),
      replace(0),
      robjsent(false),
      estimate(false),
      listing(ListingNone),
      nodata(false),
      nextfile(false),
      openerror(false),
      pluginobject(false),
      pluginobjectsent(false),
      readacl(false),
      readxattr(false),
      fname(PM_FNAME),
      lname(PM_FNAME),
      robjbuf(NULL),
      plugin_obj_cat(PM_FNAME),
      plugin_obj_type(PM_FNAME),
      plugin_obj_name(PM_FNAME),
      plugin_obj_src(PM_FNAME),
      plugin_obj_uuid(PM_FNAME),
      plugin_obj_size(PM_FNAME),
      acldatalen(0),
      acldata(PM_MESSAGE),
      xattrdatalen(0),
      xattrdata(PM_MESSAGE),
      metadatas_list(10, true)
{
   /* TODO: we have a ctx variable stored internally, decide if we use it
    * for every method or rip it off as not required in our code */
   ctx = bpctx;
}

/*
 * Main PLUGIN class destructor, handles variable freeing on delete.
 *
 * in:
 *    none
 * out:
 *    freed internal variables and class allocated during job execution
 */
METAPLUGIN::~METAPLUGIN()
{
   /* free standard variables */
   free_and_null_pool_memory(robjbuf);
}

/*
 * Check if a parameter (param) exist in ConfigFile variables set by user.
 *    The checking ignore case of the parameter.
 *
 * in:
 *    ini - a pointer to the ConfigFile class which has parsed user parameters
 *    param - a parameter to search in ini parameter keys
 * out:
 *    -1 - when a parameter param is not found in ini keys
 *    <n> - whan a parameter param is found and <n> is an index in ini->items table
 */
int METAPLUGIN::check_ini_param(char *param)
{
   if (ini.items){
      for (int k = 0; ini.items[k].name; k++){
         if (ini.items[k].found && strcasecmp(param, ini.items[k].name) == 0){
            return k;
         }
      }
   }

   return -1;
}

/*
 * Search if parameter (param) is on parameter list prepared for backend.
 *    The checking ignore case of the parameter.
 *
 * in:
 *    param - the parameter which we are looking for
 *    params - the list of parameters to search
 * out:
 *    True when the parameter param is found in list
 *    False when we can't find the param on list
 */
bool METAPLUGIN::check_plugin_param(const char *param, alist *params)
{
   POOLMEM *par;
   char *equal;
   bool found = false;

   foreach_alist(par, params){
      equal = strchr(par, '=');
      if (equal){
         /* temporary terminate the par at parameter name */
         *equal = '\0';
         if (strcasecmp(par, param) == 0){
            found = true;
         }
         /* restore parameter equal sign */
         *equal = '=';
      } else {
         if (strcasecmp(par, param) == 0){
            found = true;
         }
      }
   }
   return found;
}

/*
 * Counts the number of ini->items available as it is a NULL terminated array.
 *
 * in:
 *    ini - a pointer to ConfigFile class
 * out:
 *    <n> - the number of ini->items
 */
int METAPLUGIN::get_ini_count()
{
   int count = 0;

   if (ini.items){
      for (int k = 0; ini.items[k].name; k++){
         if (ini.items[k].found){
            count++;
         }
      }
   }

   return count;
}

/*
 *
 */
bRC METAPLUGIN::render_param(bpContext* ctx, POOLMEM *param, INI_ITEM_HANDLER *handler, char *key, item_value val)
{
   if (handler == ini_store_str){
      Mmsg(&param, "%s=%s\n", key, val.strval);
   } else
   if (handler == ini_store_int64){
      Mmsg(&param, "%s=%lld\n", key, val.int64val);
   } else
   if (handler == ini_store_bool){
      Mmsg(&param, "%s=%d\n", key, val.boolval ? 1 : 0);
   } else {
      DMSG1(ctx, DERROR, "Unsupported parameter handler for: %s\n", key);
      JMSG1(ctx, M_FATAL, "Unsupported parameter handler for: %s\n", key);
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Parsing a plugin command.
 *
 * in:
 *    bpContext - Bacula Plugin context structure
 *    command - plugin command string to parse
 * out:
 *    bRC_OK - on success
 *    bRC_Error - on error
 */
bRC METAPLUGIN::parse_plugin_command(bpContext *ctx, const char *command, alist *params)
{
   bool found;
   int count;
   int parargc, argc;
   POOLMEM *param;

   DMSG(ctx, DINFO, "Parse command: %s\n", command);
   if (parser.parse_cmd(command) != bRC_OK)
   {
      DMSG0(ctx, DERROR, "Unable to parse Plugin command line.\n");
      JMSG0(ctx, M_FATAL, "Unable to parse Plugin command line.\n");
      return bRC_Error;
   }

   /* count the numbers of user parameters if any */
   count = get_ini_count();

   /* the first (zero) parameter is a plugin name, we should skip it */
   argc = parser.argc - 1;
   parargc = argc + count;
   /* first parameters from plugin command saved during backup */
   for (int i = 1; i < parser.argc; i++) {
      param = get_pool_memory(PM_FNAME);
      found = false;

      int k;
      /* check if parameter overloaded by restore parameter */
      if ((k = check_ini_param(parser.argk[i])) != -1){
         found = true;
         DMSG1(ctx, DINFO, "parse_plugin_command: %s found in restore parameters\n", parser.argk[i]);
         if (render_param(ctx, param, ini.items[k].handler, parser.argk[i], ini.items[k].val) != bRC_OK){
            free_and_null_pool_memory(param);
            return bRC_Error;
         }
         params->append(param);
         parargc--;
      }

      /* check if param overloaded above */
      if (!found){
         if (parser.argv[i]){
            Mmsg(&param, "%s=%s\n", parser.argk[i], parser.argv[i]);
            params->append(param);
         } else {
            Mmsg(&param, "%s=1\n", parser.argk[i]);
            params->append(param);
         }
      }
      /* param is always ended with '\n' */
      DMSG(ctx, DINFO, "Param: %s", param);

      /* scan for abort_on_error parameter */
      if (strcasecmp(parser.argk[i], "abort_on_error") == 0){
         /* found, so check the value if provided, I only check the first char */
         if (parser.argv[i] && *parser.argv[i] == '0'){
            backend.ctx->clear_abort_on_error();
         } else {
            backend.ctx->set_abort_on_error();
         }
         DMSG1(ctx, DINFO, "abort_on_error found: %s\n", backend.ctx->is_abort_on_error() ? "True" : "False");
      }
      /* scan for listing parameter, so the estimate job should be executed as a Listing procedure */
      if (strcasecmp(parser.argk[i], "listing") == 0){
         /* found, so check the value if provided */
         if (parser.argv[i]){
            listing = ListingMode;
            DMSG0(ctx, DINFO, "listing procedure param found\n");
         }
      }
      /* scan for query parameter, so the estimate job should be executed as a QueryParam procedure */
      if (strcasecmp(parser.argk[i], "query") == 0){
         /* found, so check the value if provided */
         if (parser.argv[i]){
            listing = ListingQueryParams;
            DMSG0(ctx, DINFO, "query procedure param found\n");
         }
      }
   }
   /* check what was missing in plugin command but get from ini file */
   if (argc < parargc){
      for (int k = 0; ini.items[k].name; k++){
         if (ini.items[k].found && !check_plugin_param(ini.items[k].name, params)){
            param = get_pool_memory(PM_FNAME);
            DMSG1(ctx, DINFO, "parse_plugin_command: %s from restore parameters\n", ini.items[k].name);
            if (render_param(ctx, param, ini.items[k].handler, (char*)ini.items[k].name, ini.items[k].val) != bRC_OK){
               free_and_null_pool_memory(param);
               return bRC_Error;
            }
            params->append(param);
            /* param is always ended with '\n' */
            DMSG(ctx, DINFO, "Param: %s", param);
         }
      }
   }

   return bRC_OK;
}

/*
 * Parse a Restore Object saved during backup and modified by user during restore.
 *    Every RO received will generate a dedicated backend context which is used
 *    by bEventRestoreCommand to handle backend parameters for restore.
 *
 * in:
 *    bpContext - Bacula Plugin context structure
 *    rop - a restore object structure to parse
 * out:
 *    bRC_OK - on success
 *    bRC_Error - on error
 */
bRC METAPLUGIN::parse_plugin_restoreobj(bpContext *ctx, restore_object_pkt *rop)
{
   if (!rop){
      return bRC_OK;    /* end of rop list */
   }

   if (!strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME)) {
      /* we have a single RO for every backend */
      backend.switch_command(rop->plugin_name);

      DMSG(ctx, DINFO, "INIcmd: %s\n", rop->plugin_name);

      ini.clear_items();
      if (!ini.dump_string(rop->object, rop->object_len))
      {
         DMSG0(ctx, DERROR, "ini->dump_string failed\n");
         JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
         return bRC_Error;
      }

      ini.register_items(plugin_items_dump, sizeof(struct ini_items));
      if (!ini.parse(ini.out_fname))
      {
         DMSG0(ctx, DERROR, "ini->parse failed\n");
         JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
         return bRC_Error;
      }

      for (int i=0; ini.items[i].name; i++)
      {
         if (ini.items[i].found){
            if (ini.items[i].handler == ini_store_str){
               DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.strval);
            } else
            if (ini.items[i].handler == ini_store_int64){
               DMSG2(ctx, DINFO, "INI: %s = %lld\n", ini.items[i].name, ini.items[i].val.int64val);
            } else
            if (ini.items[i].handler == ini_store_bool){
               DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.boolval ? "True" : "False");
            } else {
               DMSG1(ctx, DERROR, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
               JMSG1(ctx, M_FATAL, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
               return bRC_Error;
            }
         }
      }
   }

   return bRC_OK;
}

/*
 * Run external backend script/application using BACKEND_CMD compile variable.
 *    It will run the backend in current backend context (backendctx) and should
 *    be called when a new backend is really required only.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when backend spawned successfully
 *    bRC_Error - when Plugin cannot run backend
 */
bRC METAPLUGIN::run_backend(bpContext *ctx)
{
   BPIPE *bp;

   if (access(backend_cmd.c_str(), X_OK) < 0){
      berrno be;
      DMSG2(ctx, DERROR, "Unable to access backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
      JMSG2(ctx, M_FATAL, "Unable to access backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
      return bRC_Error;
   }
   DMSG(ctx, DINFO, "Executing: %s\n", backend_cmd.c_str());
   bp = open_bpipe(backend_cmd.c_str(), 0, "rwe");
   if (bp == NULL){
      berrno be;
      DMSG(ctx, DERROR, "Unable to run backend. Err=%s\n", be.bstrerror());
      JMSG(ctx, M_FATAL, "Unable to run backend. Err=%s\n", be.bstrerror());
      return bRC_Error;
   }
   /* setup communication channel */
   backend.ctx->set_bpipe(bp);
   DMSG(ctx, DINFO, "Backend executed at PID=%i\n", bp->worker_pid);
   return bRC_OK;
}

bRC backendctx_finish_func(PTCOMM *ptcomm, void *cp)
{
   bpContext * ctx = (bpContext*)cp;
   bRC status = bRC_OK;
   POOL_MEM cmd(PM_FNAME);
   pm_strcpy(cmd, "FINISH\n");

   if (ptcomm->write_command(ctx, cmd.addr()) < 0){
      status = bRC_Error;
   }
   if (!ptcomm->read_ack(ctx)){
      status = bRC_Error;
   }

   return status;
}

/*
 * Sends a "FINISH" command to all executed backends indicating the end of
 * "Restore loop".
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when operation was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::signal_finish_all_backends(bpContext *ctx)
{
   return backend.foreach_command_status(backendctx_finish_func, ctx);
}

/*
 * Send end job command to backend.
 *    It terminates the backend when command sending was unsuccessful, as it is
 *    the very last procedure in protocol.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    ptcomm - backend context
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC send_endjob(bpContext *ctx, PTCOMM *ptcomm)
{
   bRC status = bRC_OK;
   POOL_MEM cmd(PM_FNAME);
   pm_strcpy(cmd, "END\n");

   if (ptcomm->write_command(ctx, cmd.c_str()) < 0){
      /* error */
      status = bRC_Error;
   } else {
      if (!ptcomm->read_ack(ctx)){
         DMSG0(ctx, DERROR, "Wrong backend response to JobEnd command.\n");
         JMSG0(ctx, ptcomm->jmsg_err_level(), "Wrong backend response to JobEnd command.\n");
         status = bRC_Error;
      }
      ptcomm->signal_term(ctx);
   }
   return status;
}

/*
 * Terminates the current backend pointed by ptcomm context.
 *    The termination sequence consist of "End Job" protocol procedure and real
 *    backend process termination including communication channel close.
 *    When we'll get an error during "End Job" procedure then we inform the user
 *    and terminate the backend as usual without unnecessary formalities. :)
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when backend termination was successful, i.e. no error in
 *             "End Job" procedure
 *    bRC_Error - when backend termination encountered an error.
 */
bRC backendctx_jobend_func(PTCOMM *ptcomm, void *cp)
{
   bpContext * ctx = (bpContext*)cp;
   bRC status = bRC_OK;

   if (send_endjob(ctx, ptcomm) != bRC_OK){
      /* error in end job */
      DMSG0(ctx, DERROR, "Error in EndJob.\n");
      status = bRC_Error;
   }
   int pid = ptcomm->get_backend_pid();
   DMSG(ctx, DINFO, "Terminate backend at PID=%d\n", pid)
   ptcomm->terminate(ctx);

   return status;
}

/*
 * Terminate all executed backends.
 *    Check METAPLUGIN::terminate_current_backend for more info.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when all backends termination was successful
 *    bRC_Error - when any backend termination encountered an error
 */
bRC METAPLUGIN::terminate_all_backends(bpContext *ctx)
{
   return backend.foreach_command_status(backendctx_jobend_func, ctx);
}

/*
 * Send a "Job Info" protocol procedure parameters.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    type - a char compliant with the protocol indicating what jobtype we run
 * out:
 *    bRC_OK - when send job info was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_jobinfo(bpContext *ctx, char type)
{
   int32_t rc;
   POOL_MEM cmd;
   char lvl;

   /* we will be sending Job Info data */
   pm_strcpy(cmd, "Job\n");
   rc = backend.ctx->write_command(ctx, cmd);
   if (rc < 0){
      /* error */
      return bRC_Error;
   }
   /* required parameters */
   Mmsg(cmd, "Name=%s\n", JobName);
   rc = backend.ctx->write_command(ctx, cmd);
   if (rc < 0){
      /* error */
      return bRC_Error;
   }
   Mmsg(cmd, "JobID=%i\n", JobId);
   rc = backend.ctx->write_command(ctx, cmd);
   if (rc < 0){
      /* error */
      return bRC_Error;
   }
   Mmsg(cmd, "Type=%c\n", type);
   rc = backend.ctx->write_command(ctx, cmd);
   if (rc < 0){
      /* error */
      return bRC_Error;
   }
   /* optional parameters */
   if (mode != RESTORE){
      switch (mode){
         case BACKUP_FULL:
            lvl = 'F';
            break;
         case BACKUP_DIFF:
            lvl = 'D';
            break;
         case BACKUP_INCR:
            lvl = 'I';
            break;
         default:
            lvl = 0;
      }
      if (lvl){
         Mmsg(cmd, "Level=%c\n", lvl);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
         return bRC_Error;
         }
      }
   }
   if (since){
      Mmsg(cmd, "Since=%ld\n", since);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
   }
   if (where){
      Mmsg(cmd, "Where=%s\n", where);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
   }
   if (regexwhere){
      Mmsg(cmd, "RegexWhere=%s\n", regexwhere);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
   }
   if (replace){
      Mmsg(cmd, "Replace=%c\n", replace);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
   }

   if (CUSTOMNAMESPACE){
      Mmsg(cmd, "Namespace=%s\n", PLUGINNAMESPACE);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
   }
   backend.ctx->signal_eod(ctx);

   if (!backend.ctx->read_ack(ctx)){
      DMSG0(ctx, DERROR, "Wrong backend response to Job command.\n");
      JMSG0(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to Job command.\n");
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Send a "Plugin Parameters" protocol procedure data.
 *    It parse plugin command and ini parameters before sending it to backend.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    command - a Plugin command for a job
 * out:
 *    bRC_OK - when send parameters was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_parameters(bpContext *ctx, char *command)
{
   int32_t rc;
   bRC status = bRC_OK;
   POOL_MEM cmd(PM_FNAME);
   alist params(16, not_owned_by_alist);
   POOLMEM *param;
   bool found;

#ifdef DEVELOPER
   static const char *regress_valid_params[] =
   {
      // add special test_backend commands to handle regression tests
      "regress_error_plugin_params",
      "regress_error_start_job",
      "regress_error_backup_no_files",
      "regress_error_backup_stderr",
      "regress_error_estimate_stderr",
      "regress_error_listing_stderr",
      "regress_error_restore_stderr",
      "regress_backup_plugin_objects",
      "regress_backup_other_file",
      "regress_error_backup_abort",
      "regress_metadata_support",
      NULL,
   };
#endif

   /* parse and prepare final backend plugin params */
   status = parse_plugin_command(ctx, command, &params);
   if (status != bRC_OK){
      /* error */
      goto bailout;
   }

   /* send backend info that parameters are coming */
   pm_strcpy(cmd, "Params\n");
   rc = backend.ctx->write_command(ctx, cmd.c_str());
   if (rc < 0){
      /* error */
      status = bRC_Error;
      goto bailout;
   }
   /* send all prepared parameters */
   foreach_alist(param, &params){
      // check valid parameter list
      found = false;
      for (int a = 0; valid_params[a] != NULL; a++ )
      {
         DMSG3(ctx, DVDEBUG, "=> '%s' vs '%s' [%d]\n", param, valid_params[a], strlen(valid_params[a]));
         if (strncasecmp(param, valid_params[a], strlen(valid_params[a])) == 0){
            found = true;
            break;
         }
      }

#ifdef DEVELOPER
      if (!found){
         // now handle regression tests commands
         for (int a = 0; regress_valid_params[a] != NULL; a++ ){
            DMSG3(ctx, DVDEBUG, "regress=> '%s' vs '%s' [%d]\n", param, regress_valid_params[a], strlen(regress_valid_params[a]));
            if (strncasecmp(param, regress_valid_params[a], strlen(regress_valid_params[a])) == 0){
               found = true;
               break;
            }
         }
      }
#endif

      // signal error if required
      if (!found){
         pm_strcpy(cmd, param);
         strip_trailing_junk(cmd.c_str());
         DMSG1(ctx, DERROR, "Unknown parameter %s in Plugin command.\n", cmd.c_str());
         JMSG1(ctx, M_ERROR, "Unknown parameter %s in Plugin command.\n", cmd.c_str());
      }

      rc = backend.ctx->write_command(ctx, param);
      if (rc < 0){
         /* error */
         status = bRC_Error;
         goto bailout;
      }
   }
   /* signal end of parameters block */
   backend.ctx->signal_eod(ctx);
   /* ack Params command */
   if (!backend.ctx->read_ack(ctx)){
      DMSG0(ctx, DERROR, "Wrong backend response to Params command.\n");
      JMSG0(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to Params command.\n");
      status = bRC_Error;
   }

bailout:
   foreach_alist(param, &params){
      free_and_null_pool_memory(param);
   }
   return status;
}

/*
 * Send start job command pointed by command variable.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    command - the command string to send
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_startjob(bpContext *ctx, const char *command)
{
   POOL_MEM cmd;

   pm_strcpy(cmd, command);
   if (backend.ctx->write_command(ctx, cmd) < 0){
      /* error */
      return bRC_Error;
   }

   if (!backend.ctx->read_ack(ctx)){
      strip_trailing_newline(cmd.c_str());
      DMSG(ctx, DERROR, "Wrong backend response to %s command.\n", cmd.c_str());
      JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to %s command.\n", cmd.c_str());
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Send "BackupStart" protocol command.
 *    more info at METAPLUGIN::send_startjob
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_startbackup(bpContext *ctx)
{
   return send_startjob(ctx, "BackupStart\n");
}

/*
 * Send "EstimateStart" protocol command.
 *    more info at METAPLUGIN::send_startjob
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_startestimate(bpContext *ctx)
{
   return send_startjob(ctx, "EstimateStart\n");
}

/*
 * Send "ListingStart" protocol command.
 *    more info at METAPLUGIN::send_startjob
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_startlisting(bpContext *ctx)
{
   return send_startjob(ctx, "ListingStart\n");
}

/*
 * Send "RestoreStart" protocol command.
 *    more info at METAPLUGIN::send_startjob
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when send command was successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::send_startrestore(bpContext *ctx)
{
   int32_t rc;
   POOL_MEM cmd(PM_FNAME);
   const char * command = "RestoreStart\n";
   POOL_MEM extpipename(PM_FNAME);

   pm_strcpy(cmd, command);
   rc = backend.ctx->write_command(ctx, cmd);
   if (rc < 0){
      /* error */
      return bRC_Error;
   }

   if (backend.ctx->read_command(ctx, cmd) < 0){
      DMSG(ctx, DERROR, "Wrong backend response to %s command.\n", command);
      JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to %s command.\n", command);
      return bRC_Error;
   }
   if (backend.ctx->is_eod()){
      /* got EOD so the backend is ready for restore */
      return bRC_OK;
   }

   /* here we expect a PIPE: command only */
   if (scan_parameter_str(cmd, "PIPE:", extpipename)){
      /* got PIPE: */
      DMSG(ctx, DINFO, "PIPE:%s\n", extpipename.c_str());
      backend.ctx->set_extpipename(extpipename.c_str());
      /* TODO: decide if plugin should verify if extpipe is available */
      pm_strcpy(cmd, "OK\n");
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      return bRC_OK;
   }
   return bRC_Error;
}

/*
 * Switches current backend context or executes new one when required.
 *    The backend (application path) to execute is set in BACKEND_CMD compile
 *    variable and handled by METAPLUGIN::run_backend() method. Just before new
 *    backend execution the method search for already spawned backends which
 *    handles the same Plugin command and when found the current backend context
 *    is switched to already available on list.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    command - the Plugin command for which we execute a backend
 * out:
 *    bRC_OK - success and backendctx has a current backend context and Plugin
 *             should send an initialization procedure
 *    bRC_Max - when was already prepared and initialized, so no
 *              reinitialization required
 *    bRC_Error - error in switching or running the backend
 */
bRC METAPLUGIN::switch_or_run_backend(bpContext *ctx, char *command)
{
   DMSG0(ctx, DINFO, "Switch or run Backend.\n");
   backend.switch_command(command);

   /* check if we have the backend with the same command already */
   if (backend.ctx->is_open())
   {
      /* and its open, so skip running the new */
      DMSG0(ctx, DINFO, "Backend already prepared.\n");
      return bRC_Max;
   }

   // now execute a backend
   if (run_backend(ctx) != bRC_OK){
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Prepares the backend for Backup/Restore/Estimate loops.
 *    The preparation consist of backend execution and initialization, required
 *    protocol initialize procedures: "Handshake", "Job Info",
 *    "Plugin Parameters" and "Start Backup"/"Start Restore"/"Start Estimate"
 *    depends on that job it is.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    command - a Plugin command from FileSet to start the job
 * out:
 *    bRC_OK - when backend is operational and prepared for job or when it is
 *             not our plugin command, anyway a success
 *    bRC_Error - encountered any error during preparation
 */
bRC METAPLUGIN::prepare_backend(bpContext *ctx, char type, char *command)
{
   // check if it is our Plugin command
   if (!isourplugincommand(PLUGINPREFIX, command) != 0){
      // it is not our plugin prefix
      return bRC_OK;
   }

   // check for prohibitted command duplication
   if (type != BACKEND_JOB_INFO_RESTORE && backend.check_command(command))
   {
      // already exist, report
      DMSG1(ctx, DERROR, "Plugin command=%s already defined, cannot proceed.\n", command);
      JMSG1(ctx, M_FATAL, "Plugin command already defined: \"%s\" Cannot proceed. You should correct FileSet configuration.\n", command);
      terminate_all_backends(ctx);
      return bRC_Error;
   }

   /* switch backend context, so backendctx has all required variables available */
   bRC status = switch_or_run_backend(ctx, command);
   if (status == bRC_Max){
      /* already prepared, skip rest of preparation */
      return bRC_OK;
   }
   if (status != bRC_OK){
      /* we have some error here */
      return bRC_Error;
   }
   /* handshake (1) */
   DMSG0(ctx, DINFO, "Backend handshake...\n");
   if (!backend.ctx->handshake(ctx, PLUGINNAME, PLUGINAPI))
   {
      backend.ctx->terminate(ctx);
      return bRC_Error;
   }
   /* Job Info (2) */
   DMSG0(ctx, DINFO, "Job Info (2) ...\n");
   if (send_jobinfo(ctx, type) != bRC_OK){
      backend.ctx->terminate(ctx);
      return bRC_Error;
   }
   /* Plugin Params (3) */
   DMSG0(ctx, DINFO, "Plugin Params (3) ...\n");
   if (send_parameters(ctx, command) != bRC_OK){
      backend.ctx->terminate(ctx);
      return bRC_Error;
   }
   switch (type){
      case BACKEND_JOB_INFO_BACKUP:
         /* Start Backup (4) */
         DMSG0(ctx, DINFO, "Start Backup (4) ...\n");
         if (send_startbackup(ctx) != bRC_OK){
            backend.ctx->terminate(ctx);
            return bRC_Error;
         }
         break;
      case BACKEND_JOB_INFO_ESTIMATE:
         /* Start Estimate or Listing (4) */
         if (listing != ListingNone){
            DMSG0(ctx, DINFO, "Start Listing (4) ...\n");
            if (send_startlisting(ctx) != bRC_OK){
               backend.ctx->terminate(ctx);
               return bRC_Error;
            }
         } else {
            DMSG0(ctx, DINFO, "Start Estimate (4) ...\n");
            if (send_startestimate(ctx) != bRC_OK){
               backend.ctx->terminate(ctx);
               return bRC_Error;
            }
         }
         break;
      case BACKEND_JOB_INFO_RESTORE:
         /* Start Restore (4) */
         DMSG0(ctx, DINFO, "Start Restore (4) ...\n");
         if (send_startrestore(ctx) != bRC_OK){
            backend.ctx->terminate(ctx);
            return bRC_Error;
         }
         break;
      default:
         return bRC_Error;
   }
   DMSG0(ctx, DINFO, "Prepare backend done.\n");
   return bRC_OK;
}

/*
 * This is the main method for handling events generated by Bacula.
 *    The behavior of the method depends on event type generated, but there are
 *    some events which does nothing, just return with bRC_OK. Every event is
 *    tracked in debug trace file to verify the event flow during development.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    event - a Bacula event structure
 *    value - optional event value
 * out:
 *    bRC_OK - in most cases signal success/no error
 *    bRC_Error - in most cases signal error
 *    <other> - depend on Bacula Plugin API if applied
 */
bRC METAPLUGIN::handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   bRC status;
   POOL_MEM tmp;

   switch (event->eventType) {
   case bEventJobStart:
      DMSG(ctx, D3, "bEventJobStart value=%s\n", NPRT((char *)value));
      getBaculaVar(bVarJobId, (void *)&JobId);
      getBaculaVar(bVarJobName, (void *)&JobName);
      break;

   case bEventJobEnd:
      status = bRC_OK;
      DMSG(ctx, D3, "bEventJobEnd value=%s\n", NPRT((char *)value));
      status = terminate_all_backends(ctx);
      return status;

   case bEventLevel:
      char lvl;
      lvl = (char)((intptr_t) value & 0xff);
      DMSG(ctx, D2, "bEventLevel='%c'\n", lvl);
      switch (lvl) {
         case 'F':
            DMSG0(ctx, D2, "backup level = Full\n");
            mode = BACKUP_FULL;
            break;
         case 'I':
            DMSG0(ctx, D2, "backup level = Incr\n");
            mode = BACKUP_INCR;
            break;
         case 'D':
            DMSG0(ctx, D2, "backup level = Diff\n");
            mode = BACKUP_DIFF;
            break;
         default:
            DMSG0(ctx, D2, "unsupported backup level!\n");
            return bRC_Error;
      }
      break;

   case bEventSince:
      since = (time_t) value;
      DMSG(ctx, D2, "bEventSince=%ld\n", (intptr_t) since);
      break;

   case bEventStartBackupJob:
      DMSG(ctx, D3, "bEventStartBackupJob value=%s\n", NPRT((char *)value));
      break;

   case bEventEndBackupJob:
      DMSG(ctx, D2, "bEventEndBackupJob value=%s\n", NPRT((char *)value));
      break;

   case bEventStartRestoreJob:
      DMSG(ctx, DINFO, "StartRestoreJob value=%s\n", NPRT((char *)value));
      getBaculaVar(bVarWhere, &where);
      DMSG(ctx, DINFO, "Where=%s\n", NPRT(where));
      getBaculaVar(bVarRegexWhere, &regexwhere);
      DMSG(ctx, DINFO, "RegexWhere=%s\n", NPRT(regexwhere));
      getBaculaVar(bVarReplace, &replace);
      DMSG(ctx, DINFO, "Replace=%c\n", replace);
      mode = RESTORE;
      break;

   case bEventEndRestoreJob:
      DMSG(ctx, DINFO, "bEventEndRestoreJob value=%s\n", NPRT((char *)value));
      return signal_finish_all_backends(ctx);

   /* Plugin command e.g. plugin = <plugin-name>:parameters */
   case bEventEstimateCommand:
      DMSG(ctx, D1, "bEventEstimateCommand value=%s\n", NPRT((char *)value));
      estimate = true;
      return prepare_backend(ctx, BACKEND_JOB_INFO_ESTIMATE, (char*)value);

   /* Plugin command e.g. plugin = <plugin-name>:parameters */
   case bEventBackupCommand:
      DMSG(ctx, D2, "bEventBackupCommand value=%s\n", NPRT((char *)value));
      robjsent = false;
      return prepare_backend(ctx, BACKEND_JOB_INFO_BACKUP, (char*)value);

   /* Plugin command e.g. plugin = <plugin-name>:parameters */
   case bEventRestoreCommand:
      DMSG(ctx, D2, "bEventRestoreCommand value=%s\n", NPRT((char *)value));
      return prepare_backend(ctx, BACKEND_JOB_INFO_RESTORE, (char*)value);

   /* Plugin command e.g. plugin = <plugin-name>:parameters */
   case bEventPluginCommand:
      DMSG(ctx, D2, "bEventPluginCommand value=%s\n", NPRT((char *)value));
      if (isourplugincommand(PLUGINPREFIX, (char*)value) && !backend_available)
      {
         DMSG2(ctx, DERROR, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), backend_error.c_str());
         JMSG2(ctx, M_FATAL, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), backend_error.c_str());
         return bRC_Error;
      }
      break;

   case bEventOptionPlugin:
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
      break;

   case bEventRestoreObject:
      /* Restore Object handle - a plugin configuration for restore and user supplied parameters */
      if (!value){
         DMSG0(ctx, DINFO, "End restore objects\n");
         break;
      }
      DMSG(ctx, D2, "bEventRestoreObject value=%p\n", value);
      return parse_plugin_restoreobj(ctx, (restore_object_pkt *) value);

   case bEventCancelCommand:
      DMSG(ctx, D3, "bEventCancelCommand value=%s\n", NPRT((char *)value));
      /*
         TODO: The code can set a flag (better if protected via a global mutex), and we
         TODO: can send a kill USR1 or TERM signal to the backend if the variable is
         TODO: protected with the global mutex and available easily.
         TODO: PETITION: Our plugin (RHV WhiteBearSolutions) search the packet E CANCEL.
         TODO: If you modify this behaviour, please you notify us.
      */
      if (backend.ctx != NULL) {
         // XXX: something is going different then designed here, as backend.ctx is NULL
         bsscanf("CANCEL", "%s", tmp.c_str());
         backend.ctx->signal_error(ctx, tmp.c_str());
      }
      break;

   default:
      // enabled only for Debug
      DMSG2(ctx, D2, "Unknown event: %s (%d) \n", eventtype2str(event), event->eventType);
   }

   return bRC_OK;
}

/*
 * Make a real data read from the backend and checks for EOD.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    io - Bacula Plugin API I/O structure for I/O operations
 * out:
 *    bRC_OK - when successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::perform_read_data(bpContext *ctx, struct io_pkt *io)
{
   int rc;

   if (nodata){
      io->status = 0;
      return bRC_OK;
   }
   rc = backend.ctx->read_data_fixed(ctx, io->buf, io->count);
   if (rc < 0){
      io->status = rc;
      io->io_errno = rc;
      return bRC_Error;
   }
   io->status = rc;
   if (backend.ctx->is_eod()){
      /* TODO: we signal EOD as rc=0, so no need to explicity check for EOD, right? */
      io->status = 0;
   }
   return bRC_OK;
}

/*
 * Make a real write data to the backend.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    io - Bacula Plugin API I/O structure for I/O operations
 * out:
 *    bRC_OK - when successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::perform_write_data(bpContext *ctx, struct io_pkt *io)
{
   int rc;
   POOL_MEM cmd(PM_FNAME);

   /* check if DATA was sent */
   if (nodata){
      pm_strcpy(cmd, "DATA\n");
      rc = backend.ctx->write_command(ctx, cmd.c_str());
      if (rc < 0){
         /* error */
         io->status = rc;
         io->io_errno = rc;
         return bRC_Error;
      }
      /* DATA command sent */
      nodata = false;
   }
   DMSG1(ctx, DVDEBUG, "perform_write_data: %d\n", io->count);
   rc = backend.ctx->write_data(ctx, io->buf, io->count);
   io->status = rc;
   if (rc < 0){
      io->io_errno = rc;
      return bRC_Error;
   }
   nodata = false;
   return bRC_OK;
}

/*
 * Handle protocol "DATA" command from backend during backup loop.
 *    It handles a "no data" flag when saved file contains no data to
 *    backup (empty file).
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    io - Bacula Plugin API I/O structure for I/O operations
 * out:
 *    bRC_OK - when successful
 *    bRC_Error - on any error
 *    io->status, io->io_errno - set to error on any error
 */
bRC METAPLUGIN::perform_backup_open(bpContext *ctx, struct io_pkt *io)
{
   int rc;
   POOL_MEM cmd(PM_FNAME);

   /* expecting DATA command */
   nodata = false;
   rc = backend.ctx->read_command(ctx, cmd);
   if (backend.ctx->is_eod()){
      /* no data for file */
      nodata = true;
   } else
   // expect no error and 'DATA' starting packet
   if (rc < 0 || !bstrcmp(cmd.c_str(), "DATA")){
      io->status = rc;
      io->io_errno = EIO;
      openerror = backend.ctx->is_fatal() ? false : true;
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Signal the end of data to restore and verify acknowledge from backend.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    io - Bacula Plugin API I/O structure for I/O operations
 * out:
 *    bRC_OK - when successful
 *    bRC_Error - on any error
 */
bRC METAPLUGIN::perform_write_end(bpContext *ctx, struct io_pkt *io)
{
   if (!nodata){
      /* signal end of data to restore and get ack */
      if (!backend.ctx->send_ack(ctx)){
         io->status = -1;
         io->io_errno = EPIPE;
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/*
 * Reads ACL data from backend during backup. Save it for handleXACLdata from
 * Bacula. As we do not know if a backend will send the ACL data or not, we
 * cannot wait to read this data until handleXACLdata will be called.
 * TODO: The method has a limitation and accept up to PM_BSOCK acl data to save
 *       which is about 64kB. It should be sufficient for virtually all acl data
 *       we can imagine. But when a backend developer could expect a larger data
 *       to save he should rewrite perform_read_acl() method.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when ACL data was read successfully and this.readacl set to true
 *    bRC_Error - on any error during acl data read
 */
bRC METAPLUGIN::perform_read_acl(bpContext *ctx)
{
   DMSG0(ctx, DINFO, "perform_read_acl\n");
   acldatalen = backend.ctx->read_data(ctx, acldata);
   if (acldatalen < 0){
      DMSG0(ctx, DERROR, "Cannot read ACL data from backend.\n");
      return bRC_Error;
   }

   DMSG1(ctx, DINFO, "readACL: %i\n", acldatalen);
   if (!backend.ctx->read_ack(ctx)){
      /* should get EOD */
      DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
      return bRC_Error;
   }

   readacl = true;

   return bRC_OK;
}

/*
 * Reads XATTR data from backend during backup. Save it for handleXACLdata from
 * Bacula. As we do not know if a backend will send the XATTR data or not, we
 * cannot wait to read this data until handleXACLdata will be called.
 * TODO: The method has a limitation and accept up to PM_BSOCK xattr data to save
 *       which is about 64kB. It should be sufficient for virtually all xattr data
 *       we can imagine. But when a backend developer could expect a larger data
 *       to save he should rewrite perform_read_xattr() method.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when XATTR data was read successfully and this.readxattr set
 *             to true
 *    bRC_Error - on any error during acl data read
 */
bRC METAPLUGIN::perform_read_xattr(bpContext *ctx)
{
   DMSG0(ctx, DINFO, "perform_read_xattr\n");
   xattrdatalen = backend.ctx->read_data(ctx, xattrdata);
   if (xattrdatalen < 0){
      DMSG0(ctx, DERROR, "Cannot read XATTR data from backend.\n");
      return bRC_Error;
   }
   DMSG1(ctx, DINFO, "readXATTR: %i\n", xattrdatalen);
   if (!backend.ctx->read_ack(ctx)){
      /* should get EOD */
      DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
      return bRC_Error;
   }
   readxattr = true;
   return bRC_OK;
}

/**
 * @brief Reads metadata info from backend and adds it as a metadata packet.
 *
 * @param ctx for Bacula debug and jobinfo messages
 * @param type detected Metadata type
 * @param sp save packet
 * @return bRC bRC_OK when success, bRC_Error when some error
 */
bRC METAPLUGIN::perform_read_metadata_info(bpContext *ctx, metadata_type type, struct save_pkt *sp)
{
   POOL_MEM data(PM_MESSAGE);

   DMSG0(ctx, DINFO, "perform_read_metadata_info\n");

   int len = backend.ctx->read_data(ctx, data);
   if (len < 0){
      DMSG1(ctx, DERROR, "Cannot read METADATA(%i) information from backend.\n", type);
      return bRC_Error;
   }

   DMSG1(ctx, DINFO, "read METADATA info len: %i\n", len);
   if (!backend.ctx->read_ack(ctx)){
      /* should get EOD */
      DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
      return bRC_Error;
   }

   // Bacula API for metadata requires that a plugin
   // handle metadata buffer allocation
   POOLMEM *ptr = (POOLMEM *)bmalloc(len);
   memcpy(ptr, data.addr(), len);

   // add it to the list for reference to not lot it
   metadatas_list.append(ptr);
   metadatas.add_packet(type, len, ptr);
   sp->plug_meta = &metadatas;

   return bRC_OK;
}

/**
 * @brief Does metadata command scan and map to metadata types.
 *
 * @param cmd a command string read from backend
 * @return metadata_type returned from map
 */
metadata_type METAPLUGIN::scan_metadata_type(const POOL_MEM &cmd)
{
   DMSG1(ctx, DDEBUG, "scan_metadata_type checking: %s\n", cmd.c_str());
   for (int i = 0; plugin_metadata_map[i].command != NULL; i++)
   {
      if (bstrcmp(cmd.c_str(), plugin_metadata_map[i].command)){
         DMSG2(ctx, DDEBUG, "match: %s => %d\n", plugin_metadata_map[i].command, plugin_metadata_map[i].type);
         return plugin_metadata_map[i].type;
      }
   }

   return plugin_meta_invalid;
}

const char * METAPLUGIN::prepare_metadata_type(metadata_type type)
{
   for (int i = 0; plugin_metadata_map[i].command != NULL; i++){
      if (plugin_metadata_map[i].type == type){
         return plugin_metadata_map[i].command;
      }
   }

   return "METADATA_STREAM\n";
}

/*
 * Sends ACL data from restore stream to backend.
 * TODO: The method has a limitation and accept a single xacl_pkt call for
 *       a single file. As the restored acl stream and records are the same as
 *       was saved during backup, you can expect no more then a single PM_BSOCK
 *       and about 64kB of acl data send to backend.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    xacl_pkt - the restored ACL data for backend
 * out:
 *    bRC_OK - when ACL data was restored successfully
 *    bRC_Error - on any error during acl data restore
 */
bRC METAPLUGIN::perform_write_acl(bpContext* ctx, xacl_pkt* xacl)
{
   int rc;
   POOL_MEM cmd(PM_FNAME);

   if (xacl->count > 0){
      /* send command ACL */
      pm_strcpy(cmd, "ACL\n");
      backend.ctx->write_command(ctx, cmd.c_str());
      /* send acls data */
      DMSG1(ctx, DINFO, "writeACL: %i\n", xacl->count);
      rc = backend.ctx->write_data(ctx, xacl->content, xacl->count);
      if (rc < 0){
         /* got some error */
         return bRC_Error;
      }
      /* signal end of acls data to restore and get ack */
      if (!backend.ctx->send_ack(ctx)){
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/*
 * Sends XATTR data from restore stream to backend.
 * TODO: The method has a limitation and accept a single xacl_pkt call for
 *       a single file. As the restored acl stream and records are the same as
 *       was saved during backup, you can expect no more then a single PM_BSOCK
 *       and about 64kB of acl data send to backend.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    xacl_pkt - the restored XATTR data for backend
 * out:
 *    bRC_OK - when XATTR data was restored successfully
 *    bRC_Error - on any error during acl data restore
 */
bRC METAPLUGIN::perform_write_xattr(bpContext* ctx, xacl_pkt* xacl)
{
   POOL_MEM cmd(PM_FNAME);

   if (xacl->count > 0){
      /* send command XATTR */
      pm_strcpy(cmd, "XATTR\n");
      backend.ctx->write_command(ctx, cmd.c_str());
      /* send xattrs data */
      DMSG1(ctx, DINFO, "writeXATTR: %i\n", xacl->count);
      int rc = backend.ctx->write_data(ctx, xacl->content, xacl->count);
      if (rc < 0){
         /* got some error */
         return bRC_Error;
      }
      /* signal end of xattrs data to restore and get ack */
      if (!backend.ctx->send_ack(ctx)){
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/*
 * The method works as a dispatcher for expected commands received from backend.
 *    It handles a three commands associated with file attributes/metadata:
 *    - FNAME:... - the next file to backup
 *    - ACL - next data will be acl data, so perform_read_acl()
 *    - XATTR - next data will be xattr data, so perform_read_xattr()
 *    and additionally when no more files to backup it handles EOD.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 * out:
 *    bRC_OK - when plugin read the command, dispatched a work and setup flags
 *    bRC_Error - on any error during backup
 */
bRC METAPLUGIN::perform_read_metadata(bpContext *ctx)
{
   POOL_MEM cmd(PM_FNAME);

   DMSG0(ctx, DDEBUG, "perform_read_metadata()\n");
   // setup flags
   nextfile = pluginobject = readacl = readxattr = false;
   // loop on metadata from backend or EOD which means no more files to backup
   while (true)
   {
      if (backend.ctx->read_command(ctx, cmd) > 0){
         /* yup, should read FNAME, ACL or XATTR from backend, check which one */
         DMSG(ctx, DDEBUG, "read_command(1): %s\n", cmd.c_str());
         if (scan_parameter_str(cmd, "FNAME:", fname)){
            /* got FNAME: */
            nextfile = true;
            return bRC_OK;
         }
         if (scan_parameter_str(cmd, "PLUGINOBJ:", fname)){
            /* got Plugin Object header */
            nextfile = true;
            pluginobject = true;
            return bRC_OK;
         }
         if (bstrcmp(cmd.c_str(), "ACL")){
            /* got ACL header */
            perform_read_acl(ctx);
            continue;
         }
         if (bstrcmp(cmd.c_str(), "XATTR")){
            /* got XATTR header */
            perform_read_xattr(ctx);
            continue;
         }
         if (bstrcmp(cmd.c_str(), "FileIndex")){
            /* got FileIndex query */
            perform_file_index_query(ctx);
            continue;
         }
         /* error in protocol */
         DMSG(ctx, DERROR, "Protocol error, got unknown command: %s\n", cmd.c_str());
         JMSG(ctx, M_FATAL, "Protocol error, got unknown command: %s\n", cmd.c_str());
         return bRC_Error;
      } else {
         if (backend.ctx->is_fatal()){
            /* raise up error from backend */
            return bRC_Error;
         }
         if (backend.ctx->is_eod()){
            /* no more files to backup */
            DMSG0(ctx, DDEBUG, "No more files to backup from backend.\n");
            return bRC_OK;
         }
      }
   }

   return bRC_Error;
}

bRC METAPLUGIN::perform_file_index_query(bpContext *ctx)
{
   POOL_MEM cmd(PM_FNAME);
   int32_t fileindex;

   getBaculaVar(bVarFileIndex, (void *)&fileindex);
   Mmsg(cmd, "%d\n", fileindex);
   if (backend.ctx->write_command(ctx, cmd) < 0){
      /* error */
      return bRC_Error;
   }

   return bRC_OK;
}

/**
 * @brief
 *
 * @param ctx
 * @param sp
 * @return bRC
 */
bRC METAPLUGIN::perform_read_pluginobject(bpContext *ctx, struct save_pkt *sp)
{
   POOL_MEM cmd(PM_FNAME);

   if (strlen(fname.c_str()) == 0){
      // input variable is not valid
      return bRC_Error;
   }

   sp->plugin_obj.path = fname.c_str();
   DMSG0(ctx, DDEBUG, "perform_read_pluginobject()\n");
   // loop on plugin objects parameters from backend and EOD
   while (true){
      if (backend.ctx->read_command(ctx, cmd) > 0){
         DMSG(ctx, DDEBUG, "read_command(3): %s\n", cmd.c_str());
         if (scan_parameter_str(cmd, "PLUGINOBJ_CAT:", plugin_obj_cat)){
            DMSG1(ctx, DDEBUG, "category: %s\n", plugin_obj_cat.c_str());
            sp->plugin_obj.object_category = plugin_obj_cat.c_str();
            continue;
         }
         if (scan_parameter_str(cmd, "PLUGINOBJ_TYPE:", plugin_obj_type)){
            DMSG1(ctx, DDEBUG, "type: %s\n", plugin_obj_type.c_str());
            sp->plugin_obj.object_type = plugin_obj_type.c_str();
            continue;
         }
         if (scan_parameter_str(cmd, "PLUGINOBJ_NAME:", plugin_obj_name)){
            DMSG1(ctx, DDEBUG, "name: %s\n", plugin_obj_name.c_str());
            sp->plugin_obj.object_name = plugin_obj_name.c_str();
            continue;
         }
         if (scan_parameter_str(cmd, "PLUGINOBJ_SRC:", plugin_obj_src)){
            DMSG1(ctx, DDEBUG, "src: %s\n", plugin_obj_src.c_str());
            sp->plugin_obj.object_source = plugin_obj_src.c_str();
            continue;
         }
         if (scan_parameter_str(cmd, "PLUGINOBJ_UUID:", plugin_obj_uuid)){
            DMSG1(ctx, DDEBUG, "uuid: %s\n", plugin_obj_uuid.c_str());
            sp->plugin_obj.object_uuid = plugin_obj_uuid.c_str();
            continue;
         }
         POOL_MEM param(PM_NAME);
         if (scan_parameter_str(cmd, "PLUGINOBJ_SIZE:", param)){
            if (!size_to_uint64(param.c_str(), strlen(param.c_str()), &plugin_obj_size)){
               // error in convert
               DMSG1(ctx, DERROR, "Cannot convert Plugin Object Size to integer! p=%s\n", param.c_str());
               JMSG1(ctx, M_ERROR, "Cannot convert Plugin Object Size to integer! p=%s\n", param.c_str());
               return bRC_Error;
            }
            DMSG1(ctx, DDEBUG, "size: %llu\n", plugin_obj_size);
            sp->plugin_obj.object_size = plugin_obj_size;
            continue;
         }
         /* error in protocol */
         DMSG(ctx, DERROR, "Protocol error, got unknown command: %s\n", cmd.c_str());
         JMSG(ctx, M_FATAL, "Protocol error, got unknown command: %s\n", cmd.c_str());
         return bRC_Error;
      } else {
         if (backend.ctx->is_fatal()){
            /* raise up error from backend */
            return bRC_Error;
         }
         if (backend.ctx->is_eod()){
            /* no more plugin object params to backup */
            DMSG0(ctx, DINFO, "No more Plugin Object params from backend.\n");
            pluginobject = false;
            pluginobjectsent = true;
            return bRC_OK;
         }
      }
   }

   return bRC_Error;
}

/*
 * Handle Bacula Plugin I/O API for backend
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    io - Bacula Plugin API I/O structure for I/O operations
 * out:
 *    bRC_OK - when successful
 *    bRC_Error - on any error
 *    io->status, io->io_errno - correspond to a plugin io operation status
 */
bRC METAPLUGIN::pluginIO(bpContext *ctx, struct io_pkt *io)
{
   static int rw = 0;      // this variable handles single debug message

   /* assume no error from the very beginning */
   io->status = 0;
   io->io_errno = 0;
   switch (io->func) {
      case IO_OPEN:
         DMSG(ctx, D2, "IO_OPEN: (%s)\n", io->fname);
         switch (mode){
            case BACKUP_FULL:
            case BACKUP_INCR:
            case BACKUP_DIFF:
               return perform_backup_open(ctx, io);
            case RESTORE:
               nodata = true;
               break;
            default:
               return bRC_Error;
         }
         break;
      case IO_READ:
         if (!rw) {
            rw = 1;
            DMSG2(ctx, D2, "IO_READ buf=%p len=%d\n", io->buf, io->count);
         }
         switch (mode){
            case BACKUP_FULL:
            case BACKUP_INCR:
            case BACKUP_DIFF:
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
         switch (mode){
            case RESTORE:
               return perform_write_data(ctx, io);
            default:
               return bRC_Error;
         }
         break;
      case IO_CLOSE:
         DMSG0(ctx, D2, "IO_CLOSE\n");
         rw = 0;
         if (!backend.ctx->close_extpipe(ctx)){
            return bRC_Error;
         }
         switch (mode){
            case RESTORE:
               return perform_write_end(ctx, io);
            case BACKUP_FULL:
            case BACKUP_INCR:
            case BACKUP_DIFF:
               return perform_read_metadata(ctx);
            default:
               return bRC_Error;
         }
         break;
   }

   return bRC_OK;
}

/*
 * Unimplemented, always return bRC_OK.
 */
bRC METAPLUGIN::getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/*
 * Unimplemented, always return bRC_OK.
 */
bRC METAPLUGIN::setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/*
 * Get all required information from backend to populate save_pkt for Bacula.
 *    It handles a Restore Object (FT_PLUGIN_CONFIG) for every Full backup and
 *    new Plugin Backup Command if setup in FileSet. It uses a help from
 *    endBackupFile() handling the next FNAME command for the next file to
 *    backup. The communication protocol requires some file attributes command
 *    required it raise the error when insufficient parameters received from
 *    backend. It assumes some parameters at save_pkt struct to be automatically
 *    set like: sp->portable, sp->statp.st_blksize, sp->statp.st_blocks.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    save_pkt - Bacula Plugin API save packet structure
 * out:
 *    bRC_OK - when save_pkt prepared successfully and we have file to backup
 *    bRC_Max - when no more files to backup
 *    bRC_Error - in any error
 */
bRC METAPLUGIN::startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   POOL_MEM cmd(PM_FNAME);
   char type;
   size_t size;
   int uid, gid;
   uint perms;
   int nlinks;
   int32_t nfi;
   int reqparams = 2;

   /* The first file in Full backup, is the RestoreObject */
   if (!estimate && mode == BACKUP_FULL && robjsent == false) {
      ConfigFile ini;

      /* robj for the first time, allocate the buffer */
      if (!robjbuf){
         robjbuf = get_pool_memory(PM_FNAME);
      }

      ini.register_items(plugin_items_dump, sizeof(struct ini_items));
      sp->restore_obj.object_name = (char *)INI_RESTORE_OBJECT_NAME;
      sp->restore_obj.object_len = ini.serialize(&robjbuf);
      sp->restore_obj.object = robjbuf;
      sp->type = FT_PLUGIN_CONFIG;
      DMSG0(ctx, DINFO, "Prepared RestoreObject sent.\n");
      return bRC_OK;
   }

   // check if this is the first file from backend to backup
   if (!nextfile){
      // so read FNAME or EOD/Error
      if (perform_read_metadata(ctx) != bRC_OK){
         // signal error
         return bRC_Error;
      }
      if (!nextfile){
         // got EOD, so no files to backup at all!
         // if we return a value different from bRC_OK then Bacula will finish
         // backup process, which at first call means no files to archive
         return bRC_Max;
      }
   }
   // setup required fname in save_pkt
   DMSG(ctx, DINFO, "fname:%s\n", fname.c_str());
   sp->fname = fname.c_str();

   if (!pluginobject){
      // here we handle metadata information
      reqparams--;

      // ensure clear state for metadatas
      sp->plug_meta = NULL;
      metadatas.reset();
      metadatas_list.destroy();

      while (backend.ctx->read_command(ctx, cmd) > 0){
         DMSG(ctx, DINFO, "read_command(2): %s\n", cmd.c_str());
         // int nrscan = sscanf(cmd.c_str(), "STAT:%c %ld %d %d %o %d", &type, &size, &uid, &gid, &perms, &nlinks);
         nfi = -1;
         int nrscan = sscanf(cmd.c_str(), "STAT:%c %ld %d %d %o %d %d", &type, &size, &uid, &gid, &perms, &nlinks, &nfi);
         DMSG1(ctx, DVDEBUG, "read_command-nrscan: %d\n", nrscan);
         if (nrscan >= 6)
         {
            sp->statp.st_size = size;
            sp->statp.st_nlink = nlinks;
            sp->statp.st_uid = uid;
            sp->statp.st_gid = gid;
            sp->statp.st_mode = perms;
            switch (type)
            {
            case 'F':
               sp->type = FT_REG;
               break;
            case 'E':
               sp->type = FT_REGE;
               break;
            case 'D':
               sp->type = FT_DIREND;
               sp->link = sp->fname;
               break;
            case 'S':
               sp->type = FT_LNK;
               reqparams++;
               break;
            case 'L':
               if (nrscan > 6){
                  sp->type = FT_LNKSAVED;
                  sp->LinkFI = nfi;
               } else {
                  DMSG1(ctx, DERROR, "Invalid stat packet: %s\n", cmd.c_str());
                  JMSG1(ctx, backend.ctx->jmsg_err_level(), "Invalid stat packet: %s\n", cmd.c_str());
                  return bRC_Error;
               }
               break;
            default:
               /* we need to signal error */
               sp->type = FT_REG;
               DMSG2(ctx, DERROR, "Invalid file type: %c for %s\n", type, fname.c_str());
               JMSG2(ctx, M_ERROR, "Invalid file type: %c for %s\n", type, fname.c_str());
            }
            DMSG4(ctx, DINFO, "STAT:%c size:%lld uid:%d gid:%d\n", type, size, uid, gid);
            DMSG3(ctx, DINFO, " mode:%06o nl:%d fi:%d\n", perms, nlinks, nfi);
            reqparams--;
            continue;
         }
         if (bsscanf(cmd.c_str(), "TSTAMP:%ld %ld %ld", &sp->statp.st_atime, &sp->statp.st_mtime, &sp->statp.st_ctime) == 3){
            DMSG3(ctx, DINFO, "TSTAMP:%ld(at) %ld(mt) %ld(ct)\n", sp->statp.st_atime, sp->statp.st_mtime, sp->statp.st_ctime);
            continue;
         }
         if (scan_parameter_str(cmd, "LSTAT:", lname) == 1) {
            sp->link = lname.c_str();
            reqparams--;
            DMSG(ctx, DINFO, "LSTAT:%s\n", lname.c_str());
            continue;
         }
         POOL_MEM tmp(PM_FNAME);
         if (scan_parameter_str(cmd, "PIPE:", tmp)) {
            /* handle PIPE command */
            DMSG(ctx, DINFO, "read pipe at: %s\n", tmp.c_str());
            int extpipe = open(tmp.c_str(), O_RDONLY);
            if (extpipe > 0) {
               DMSG0(ctx, DINFO, "ExtPIPE file available.\n");
               backend.ctx->set_extpipe(extpipe);
               pm_strcpy(tmp, "OK\n");
               backend.ctx->write_command(ctx, tmp.c_str());
            } else {
               /* here are common error signaling */
               berrno be;
               DMSG(ctx, DERROR, "ExtPIPE file open error! Err=%s\n", be.bstrerror());
               JMSG(ctx, backend.ctx->jmsg_err_level(), "ExtPIPE file open error! Err=%s\n", be.bstrerror());
               pm_strcpy(tmp, "Err\n");
               backend.ctx->signal_error(ctx, tmp.c_str());
               return bRC_Error;
            }
            continue;
         }
         metadata_type mtype = scan_metadata_type(cmd);
         if (mtype != plugin_meta_invalid) {
            DMSG1(ctx, DDEBUG, "metaData handling: %d\n", mtype);
            if (perform_read_metadata_info(ctx, mtype, sp) != bRC_OK) {
               DMSG0(ctx, DERROR, "Cannot perform_read_metadata_info!\n");
               JMSG0(ctx, backend.ctx->jmsg_err_level(), "Cannot perform_read_metadata_info!\n");
               return bRC_Error;
            }
            continue;
         } else {
            DMSG1(ctx, DERROR, "Invalid File Attributes command: %s\n", cmd.c_str());
            JMSG1(ctx, backend.ctx->jmsg_err_level(), "Invalid File Attributes command: %s\n", cmd.c_str());
            return bRC_Error;
         }
      }

      DMSG0(ctx, DINFO, "File attributes end.\n");
      if (reqparams > 0) {
         DMSG0(ctx, DERROR, "Protocol error, not enough file attributes from backend.\n");
         JMSG0(ctx, M_FATAL, "Protocol error, not enough file attributes from backend.\n");
         return bRC_Error;
      }

   } else {
      // handle Plugin Object parameters
      if (perform_read_pluginobject(ctx, sp) != bRC_OK) {
         // signal error
         return bRC_Error;
      }
      sp->type = FT_PLUGIN_OBJECT;
      size = sp->plugin_obj.object_size;
   }

   if (backend.ctx->is_error()) {
      return bRC_Error;
   }

   sp->portable = true;
   sp->statp.st_blksize = 4096;
   sp->statp.st_blocks = size / 4096 + 1;

   // DMSG1(ctx, DVDEBUG, "sp->plug_meta = %p\n", sp->plug_meta);

   return bRC_OK;
}

/*
 * Check for a next file to backup or the end of the backup loop.
 *    The next file to backup is indicated by a FNAME command from backend and
 *    no more files to backup as EOD. It helps startBackupFile handling FNAME
 *    for next file.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    save_pkt - Bacula Plugin API save packet structure
 * out:
 *    bRC_OK - when no more files to backup
 *    bRC_More - when Bacula should expect a next file
 *    bRC_Error - in any error
 */
bRC METAPLUGIN::endBackupFile(bpContext *ctx)
{
   POOL_MEM cmd(PM_FNAME);

   if (!estimate){
      /* The current file was the restore object, so just ask for the next file */
      if (mode == BACKUP_FULL && robjsent == false) {
         robjsent = true;
         return bRC_More;
      }
   }

   // check for next file only when no previous error
   if (!openerror)
   {
      if (estimate || pluginobjectsent)
      {
         pluginobjectsent = false;
         if (perform_read_metadata(ctx) != bRC_OK)
         {
            /* signal error */
            return bRC_Error;
         }
      }

      if (nextfile)
      {
         DMSG1(ctx, DINFO, "nextfile %s backup!\n", fname.c_str());
         return bRC_More;
      }
   }

   return bRC_OK;
}

/*
 * The PLUGIN is using this callback to handle Core restore.
 */
bRC METAPLUGIN::startRestoreFile(bpContext *ctx, const char *cmd)
{
   if (CORELOCALRESTORE && islocalpath(where)){
      DMSG0(ctx, DDEBUG, "Forwarding restore to Core\n");
      return bRC_Core;
   }
   return bRC_OK;
}

/*
 * The PLUGIN is not using this callback to handle restore.
 */
bRC METAPLUGIN::endRestoreFile(bpContext *ctx)
{
   return bRC_OK;
}

/*
 * Prepares a file to restore attributes based on data from restore_pkt.
 * It handles a response from backend to show if
 *
 * in:
 *    bpContext - bacula plugin context
 *    restore_pkt - Bacula Plugin API restore packet structure
 * out:
 *    bRC_OK - when success reported from backend
 *    rp->create_status = CF_EXTRACT - the backend will restore the file
 *                                     with pleasure
 *    rp->create_status = CF_SKIP - the backend wants to skip restoration, i.e.
 *                                  the file already exist and Replace=n was set
 *    bRC_Error, rp->create_status = CF_ERROR - in any error
 */
bRC METAPLUGIN::createFile(bpContext *ctx, struct restore_pkt *rp)
{
   POOL_MEM cmd(PM_FNAME);
   char type;

   /* FNAME:$fname$ */
   bsnprintf(cmd.c_str(), cmd.size(), "FNAME:%s\n", rp->ofname);
   backend.ctx->write_command(ctx, cmd);
   DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
   /* STAT:... */
   switch (rp->type)
   {
   case FT_REGE:
      type = 'E';
      break;
   case FT_DIREND:
      type = 'D';
      break;
   case FT_LNK:
      type = 'S';
      break;
   case FT_LNKSAVED:
      type = 'L';
      break;
   case FT_REG:
   default:
      type = 'F';
      break;
   }
   // bsnprintf(cmd.c_str(), cmd.size(), "STAT:%c %lld %d %d %06o %d %d\n", type, rp->statp.st_size,
   //           rp->statp.st_uid, rp->statp.st_gid, rp->statp.st_mode, (int)rp->statp.st_nlink, rp->LinkFI);
   Mmsg(cmd, "STAT:%c %lld %d %d %06o %d %d\n", type, rp->statp.st_size, rp->statp.st_uid, rp->statp.st_gid,
             rp->statp.st_mode, (int)rp->statp.st_nlink, rp->LinkFI);
   backend.ctx->write_command(ctx, cmd);
   DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
   /* TSTAMP:... */
   if (rp->statp.st_atime || rp->statp.st_mtime || rp->statp.st_ctime){
      // bsnprintf(cmd.c_str(), cmd.size(), "TSTAMP:%ld %ld %ld\n", rp->statp.st_atime, rp->statp.st_mtime, rp->statp.st_ctime);
      Mmsg(cmd, "TSTAMP:%ld %ld %ld\n", rp->statp.st_atime, rp->statp.st_mtime, rp->statp.st_ctime);
      backend.ctx->write_command(ctx, cmd);
      DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
   }
   /* LSTAT:$link$ */
   if (type == 'S' && rp->olname != NULL){
      // bsnprintf(cmd.c_str(), cmd.size(), "LSTAT:%s\n", rp->olname);
      Mmsg(cmd, "LSTAT:%s\n", rp->olname);
      backend.ctx->write_command(ctx, cmd);
      DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
   }
   backend.ctx->signal_eod(ctx);

   /* check if backend accepted the file */
   if (backend.ctx->read_command(ctx, cmd) > 0){
      DMSG(ctx, DINFO, "createFile:resp: %s\n", cmd.c_str());
      if (strcmp(cmd.c_str(), "OK") == 0){
         rp->create_status = CF_EXTRACT;
      } else
      if (strcmp(cmd.c_str(), "SKIP") == 0){
         rp->create_status = CF_SKIP;
      } else {
         DMSG(ctx, DERROR, "Wrong backend response to create file, got: %s\n", cmd.c_str());
         JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to create file, got: %s\n", cmd.c_str());
         rp->create_status = CF_ERROR;
         return bRC_Error;
      }
   } else {
      if (backend.ctx->is_error()){
         /* raise up error from backend */
         rp->create_status = CF_ERROR;
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/*
 * Unimplemented, always return bRC_OK.
 */
bRC METAPLUGIN::setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

/**
 * @brief
 *
 * @param ctx
 * @param exepath
 * @return bRC
 */
void METAPLUGIN::setup_backend_command(bpContext *ctx, POOL_MEM &exepath)
{
   DMSG(ctx, DINFO, "ExePath: %s\n", exepath.c_str());
   Mmsg(backend_cmd, "%s/%s", exepath.c_str(), BACKEND_CMD);
   DMSG(ctx, DINFO, "BackendPath: %s\n", backend_cmd.c_str());
   if (access(backend_cmd.c_str(), X_OK) < 0)
   {
      berrno be;
      DMSG2(ctx, DERROR, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
      pm_strcpy(backend_error, be.bstrerror());
      backend_available = false;
   } else {
      DMSG0(ctx, DINFO, "Backend available\n");
      backend_available = true;
   }
}

/**
 * @brief
 *
 * @param ctx
 * @param xacl
 * @return bRC
 */
bRC METAPLUGIN::handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   switch (xacl->func){
      case BACL_BACKUP:
         if (readacl){
            DMSG0(ctx, DINFO, "bacl_backup\n");
            xacl->count = acldatalen;
            xacl->content = acldata.c_str();
            readacl= false;
         } else {
            xacl->count = 0;
         }
         break;
      case BACL_RESTORE:
         DMSG0(ctx, DINFO, "bacl_restore\n");
         return perform_write_acl(ctx, xacl);
      case BXATTR_BACKUP:
         if (readxattr){
            DMSG0(ctx, DINFO, "bxattr_backup\n");
            xacl->count = xattrdatalen;
            xacl->content = xattrdata.c_str();
            readxattr= false;
         } else {
            xacl->count = 0;
         }
         break;
      case BXATTR_RESTORE:
         DMSG0(ctx, DINFO, "bxattr_restore\n");
         return perform_write_xattr(ctx, xacl);
   }

   return bRC_OK;
}

/*
 * QueryParameter interface
 */
bRC METAPLUGIN::queryParameter(bpContext *ctx, struct query_pkt *qp)
{
   bRC ret = bRC_More;
   OutputWriter ow(qp->api_opts);
   POOL_MEM cmd(PM_MESSAGE);
   char *p, *q, *t;
   alist values(10, not_owned_by_alist);
   key_pair *kp;

   DMSG0(ctx, D1, "METAPLUGIN::queryParameter\n");

   if (listing == ListingNone){
      listing = ListingQueryParams;
      Mmsg(cmd, "%s query=%s", qp->command, qp->parameter);
      if (prepare_backend(ctx, BACKEND_JOB_INFO_ESTIMATE, cmd.c_str()) == bRC_Error){
         return bRC_Error;
      }
   }

   /* read backend response */
   if (backend.ctx->read_command(ctx, cmd) < 0){
      DMSG(ctx, DERROR, "Cannot read backend query response for %s command.\n", qp->parameter);
      JMSG(ctx, backend.ctx->jmsg_err_level(), "Cannot read backend query response for %s command.\n", qp->parameter);
      return bRC_Error;
   }

   /* check EOD */
   if (backend.ctx->is_eod()){
      /* got EOD so the backend finish response, so terminate the chat */
      DMSG0(ctx, D1, "METAPLUGIN::queryParameter: got EOD\n");
      backend.ctx->signal_term(ctx);
      backend.ctx->terminate(ctx);
      ret = bRC_OK;
   } else {
      /*
      * here we have:
      *    key=value[,key2=value2[,...]]
      * parameters we should decompose
      */
      p = cmd.c_str();
      while (*p != '\0'){
         q = strchr(p, ',');
         if (q != NULL){
            *q++ = '\0';
         }
         // single key=value
         DMSG(ctx, D1, "METAPLUGIN::queryParameter:scan %s\n", p);
         if ((t = strchr(p, '=')) != NULL){
            *t++ = '\0';
         } else {
            t = (char*)"";     // pointer to empty string
         }
         DMSG2(ctx, D1, "METAPLUGIN::queryParameter:pair '%s' = '%s'\n", p, t);
         if (strlen(p) > 0){
            // push values only when we have key name
            kp = New(key_pair(p, t));
            values.append(kp);
         }
         p = q != NULL ? q : (char*)"";
      }

      // if more values then one then it is a list
      if (values.size() > 1){
         DMSG0(ctx, D1, "METAPLUGIN::queryParameter: will render list\n")
         ow.start_list(qp->parameter);
      }
      // render all values
      foreach_alist(kp, &values){
         ow.get_output(OT_STRING, kp->key.c_str(), kp->value.c_str(), OT_END);
         delete kp;
      }
      if (values.size() > 1){
         ow.end_list();
      }

      /* allocate working buffer, we use robjbuf variable for that as it will be freed at dtor */
      if (!robjbuf){
         robjbuf = get_pool_memory(PM_MESSAGE);
      }

      pm_strcpy(&robjbuf, ow.get_output(OT_END));
      qp->result = robjbuf;
   }

   return ret;
}

/**
 * @brief Sends metadata to backend for restore.
 *
 * @param ctx for Bacula debug and jobinfo messages
 * @param mp
 * @return bRC
 */
bRC METAPLUGIN::metadataRestore(bpContext *ctx, struct meta_pkt *mp)
{
   POOL_MEM cmd(PM_FNAME);

   if (mp->buf != NULL && mp->buf_len > 0){
      /* send command METADATA */
      pm_strcpy(cmd, prepare_metadata_type(mp->type));
      backend.ctx->write_command(ctx, cmd.c_str());
      /* send metadata stream data */
      DMSG1(ctx, DINFO, "writeMetadata: %i\n", mp->buf_len);
      int rc = backend.ctx->write_data(ctx, (char*)mp->buf, mp->buf_len);
      if (rc < 0){
         /* got some error */
         return bRC_Error;
      }
      /* signal end of metadata stream to restore and get ack */
      if (!backend.ctx->send_ack(ctx)){
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/**
 * @brief Implements default metaplugin checkFile() callback.
 *    When fname match plugin configured namespace then it return bRC_Seen by default
 *    or calls custom checkFile() callback defined by backend developer.
 *
 * @param ctx for Bacula debug and jobinfo messages
 * @param fname file name to check
 * @return bRC bRC_Seen or bRC_OK
 */
bRC METAPLUGIN::checkFile(bpContext * ctx, char *fname)
{
   if ((!CUSTOMNAMESPACE && isourpluginfname(PLUGINPREFIX, fname)) || (CUSTOMNAMESPACE && isourpluginfname(PLUGINNAMESPACE, fname)))
   {
      if (::checkFile != NULL){
         return ::checkFile(ctx, fname);
      }
      return bRC_Seen;
   }

   return bRC_OK;
}

/*
 * Called here to make a new instance of the plugin -- i.e. when
 * a new Job is started.  There can be multiple instances of
 * each plugin that are running at the same time.  Your
 * plugin instance must be thread safe and keep its own
 * local data.
 */
static bRC newPlugin(bpContext *ctx)
{
   int JobId;
   char *exepath;
   METAPLUGIN *self = New(METAPLUGIN(ctx));
   POOL_MEM exepath_clean(PM_FNAME);

   if (!self)
      return bRC_Error;

   ctx->pContext = (void*) self;

   /* setup the backend command */
   getBaculaVar(bVarExePath, (void *)&exepath);
   DMSG(ctx, DINFO, "bVarExePath: %s\n", exepath);
   pm_strcpy(exepath_clean, exepath);
   strip_trailing_slashes(exepath_clean.c_str());

   self->setup_backend_command(ctx, exepath_clean);

   getBaculaVar(bVarJobId, (void *)&JobId);
   DMSG(ctx, D1, "newPlugin JobId=%d\n", JobId);
   return bRC_OK;
}

/*
 * Release everything concerning a particular instance of
 *  a plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
   if (!ctx){
      return bRC_Error;
   }
   METAPLUGIN *self = pluginclass(ctx);
   DMSG(ctx, D1, "freePlugin this=%p\n", self);
   if (!self){
      return bRC_Error;
   }
   delete self;
   return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 *   Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   ASSERT_CTX;

   DMSG0(ctx, D3, "getPluginValue called.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->getPluginValue(ctx,var, value);
}

/*
 * Called by core code to set a plugin variable.
 *  Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   ASSERT_CTX;

   DMSG0(ctx, D3, "setPluginValue called.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->setPluginValue(ctx, var, value);
}

/*
 * Called by Bacula when there are certain events that the
 *   plugin might want to know.  The value depends on the
 *   event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   ASSERT_CTX;

   DMSG(ctx, D1, "handlePluginEvent (%i)\n", event->eventType);
   METAPLUGIN *self = pluginclass(ctx);
   return self->handlePluginEvent(ctx, event, value);
}

/*
 * Called when starting to backup a file. Here the plugin must
 *  return the "stat" packet for the directory/file and provide
 *  certain information so that Bacula knows what the file is.
 *  The plugin can create "Virtual" files by giving them
 *  a name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   ASSERT_CTX;
   if (!sp)
      return bRC_Error;

   DMSG0(ctx, D1, "startBackupFile.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->startBackupFile(ctx, sp);
}

/*
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "endBackupFile.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->endBackupFile(ctx);
}

/*
 * Called when starting restore the file, right after a createFile().
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   ASSERT_CTX;

   DMSG1(ctx, D1, "startRestoreFile: %s\n", NPRT(cmd));
   METAPLUGIN *self = pluginclass(ctx);
   return self->startRestoreFile(ctx, cmd);
}

/*
 * Done restore the file.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "endRestoreFile.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->endRestoreFile(ctx);
}

/*
 * Do actual I/O. Bacula calls this after startBackupFile
 *   or after startRestoreFile to do the actual file
 *   input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   ASSERT_CTX;

   DMSG0(ctx, DVDEBUG, "pluginIO.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->pluginIO(ctx, io);
}

/*
 * Called here to give the plugin the information needed to
 *  re-create the file on a restore.  It basically gets the
 *  stat packet that was created during the backup phase.
 *  This data is what is needed to create the file, but does
 *  not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "createFile.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->createFile(ctx, rp);
}

/*
 * Called after the file has been restored. This can be used to
 *  set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "setFileAttributes.\n");
   METAPLUGIN *self = pluginclass(ctx);
   return self->setFileAttributes(ctx, rp);
}

/*
 * handleXACLdata used for ACL/XATTR backup and restore
 */
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   ASSERT_CTX;

   DMSG(ctx, D1, "handleXACLdata: %i\n", xacl->func);
   METAPLUGIN *self = pluginclass(ctx);
   return self->handleXACLdata(ctx, xacl);
}

/* QueryParameter interface */
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp)
{
   ASSERT_CTX;

   DMSG2(ctx, D1, "queryParameter: %s:%s\n", qp->command, qp->parameter);
   METAPLUGIN *self = pluginclass(ctx);
   return self->queryParameter(ctx, qp);
}

/* Metadata Restore interface */
static bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp)
{
   ASSERT_CTX;

   DMSG2(ctx, D1, "metadataRestore: %d %d\n", mp->total_size, mp->type);
   METAPLUGIN *self = pluginclass(ctx);
   return self->metadataRestore(ctx, mp);
}

/*
 * checkFile used for accurate mode backup
 *
 * TODO: currently it is not working because a checking is performed against ldap plugin, not msad
 */
static bRC metaplugincheckFile(bpContext * ctx, char *fname)
{
   ASSERT_CTX;

   DMSG(ctx, D3, "checkFile for: %s\n", fname);
   METAPLUGIN *self = pluginclass(ctx);
   return self->checkFile(ctx, fname);
}
