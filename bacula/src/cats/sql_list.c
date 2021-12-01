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
 * Bacula Catalog Database List records interface routines
 *
 *    Written by Kern Sibbald, March 2000
 *
 */

#include  "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL

#include  "cats.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* We search resources for a specific tag, we just return the resource name
 * itself and the result can be stored in a list for example
 */
bool BDB::bdb_search_tag_records(JCR *jcr, TAG_DBR *tag, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   POOL_MEM tmp, where;
   char esc[MAX_ESCAPE_NAME_LENGTH];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   uint64_t aclbits, aclbits_extra;

   const char *name;
   const char *id;
   const char *table;

   tag->gen_sql(jcr, this, &table, &name, &id, esc, esc_name, &aclbits, &aclbits_extra);

   bdb_lock();
   pm_strcpy(where, get_acls(aclbits, true)); /* get_acls() uses a specific object buffer */
   const char *whereand = get_acls(aclbits, false);
   const char *join = get_acl_join_filter(aclbits_extra);

   if (table) {
      if (tag->all) {
         if (*esc_name) {
            /* Display all resource for a tag */
            Mmsg(tmp, "SELECT %s AS %s FROM Tag%s AS T JOIN %s USING (%s) %s WHERE T.Tag = '%s' %s",
                 name, table, table, table, id, join, esc_name, whereand);
         }
      }
      if (tag->limit > 0) {
         char ed1[50];
         pm_strcat(cmd, " LIMIT ");
         pm_strcat(cmd, edit_uint64(tag->limit, ed1));
      }
      if (!bdb_sql_query(tmp.c_str(), result_handler, ctx)) {
         bdb_unlock();
         return false;
      }
   }
   bdb_unlock();
   return true;
}

/*
 * Search Job record(s) that match JOB_DBR, the result can be stored in a alist for example
 *
 */
bool BDB::bdb_search_job_records(JCR *jcr, JOB_DBR *jr,
                                   DB_RESULT_HANDLER *handler,
                                   void *ctx)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];
   const char *join = "";
   const char *where_tmp = "";

   if (jr->Job[0] == 0) {
      return false;
   }

   bdb_lock();
   bdb_escape_string(jcr, esc, jr->Job, strlen(jr->Job));

   /* The ACL can limit on two extra tables, Client and FileSet */
   where_tmp = get_acls(DB_ACL_BIT(DB_ACL_CLIENT)  |
                        DB_ACL_BIT(DB_ACL_FILESET),
                        0);

   if (*where_tmp) {
      join = get_acl_join_filter(DB_ACL_BIT(DB_ACL_CLIENT)  |
                                 DB_ACL_BIT(DB_ACL_FILESET));
   }

   Mmsg(cmd,
        "SELECT Job "
        "FROM Job "
        " %s WHERE Job.Job ILIKE '%%%s%%' %s", join, esc, where_tmp);

   if (jr->limit > 0) {
      char ed1[50];
      pm_strcat(cmd, " LIMIT ");
      pm_strcat(cmd, edit_uint64(jr->limit, ed1));
   }

   if (!bdb_sql_query(cmd, handler, ctx)) {
      bdb_unlock();
      return false;
   }
   bdb_unlock();
   return true;
}

/* Search for a client, return only the name */
bool BDB::bdb_search_client_records(JCR *jcr, CLIENT_DBR *rec, DB_RESULT_HANDLER *callback, void *ctx)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];
   const char *where_tmp = "";

   bdb_lock();
   bdb_escape_string(jcr, esc, rec->Name, strlen(rec->Name));

   /* We can apply some ACLs for the Client table */
   where_tmp = get_acls(DB_ACL_BIT(DB_ACL_CLIENT), 0);

   Mmsg(cmd, "SELECT Name "
        "FROM Client WHERE Name ILIKE '%%%s%%' %s",
        esc, where_tmp);

   if (rec->limit > 0) {
      char ed1[50];
      pm_strcat(cmd, " LIMIT ");
      pm_strcat(cmd, edit_uint64(rec->limit, ed1));
   }

   if (!bdb_sql_query(cmd, callback, ctx)) {
      bdb_unlock();
      return false;
   }
   bdb_unlock();
   return true;
}

/*
 * Submit general SQL query
 */
int BDB::bdb_list_sql_query(JCR *jcr, const char *query, DB_LIST_HANDLER *sendit,
                      void *ctx, int verbose, e_list_type type)
{
   bdb_lock();
   if (!sql_query(query, QF_STORE_RESULT)) {
      Mmsg(errmsg, _("Query failed: %s\n"), sql_strerror());
      if (verbose) {
         sendit(ctx, errmsg);
      }
      bdb_unlock();
      return 0;
   }

   list_result(jcr,this, sendit, ctx, type);
   sql_free_result();
   bdb_unlock();
   return 1;
}

void BDB::bdb_list_pool_records(JCR *jcr, POOL_DBR *pdbr,
                     DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   bdb_lock();
   bdb_escape_string(jcr, esc, pdbr->Name, strlen(pdbr->Name));

   if (type == VERT_LIST || type == JSON_LIST) {
      if (pdbr->Name[0] != 0) {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
            "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
            "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
            "RecyclePoolId,LabelType,ActionOnPurge,CacheRetention,MaxPoolBytes, "
            "%s as PoolBytes "
            " FROM Pool WHERE Name='%s' %s",
              poolbytes[bdb_get_type_index()],
              esc, get_acl(DB_ACL_POOL, false));
      } else {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
            "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
            "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
            "RecyclePoolId,LabelType,ActionOnPurge,CacheRetention,MaxPoolBytes, "
            "%s AS PoolBytes "
            " FROM Pool %s ORDER BY PoolId",
              poolbytes[bdb_get_type_index()],
              get_acl(DB_ACL_POOL, true));
      }
   } else {
      if (pdbr->Name[0] != 0) {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool WHERE Name='%s' %s", esc, get_acl(DB_ACL_POOL, false));
      } else {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool %s ORDER BY PoolId", get_acl(DB_ACL_POOL, true));
      }
   }
   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

void BDB::bdb_list_client_records(JCR *jcr, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   bdb_lock();
   if (type == VERT_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT ClientId,Name,Uname,Plugins,AutoPrune,FileRetention,"
         "JobRetention "
           "FROM Client %s ORDER BY ClientId", get_acl(DB_ACL_CLIENT, true));
   } else {
      Mmsg(cmd, "SELECT ClientId,Name,FileRetention,JobRetention "
           "FROM Client %s ORDER BY ClientId", get_acl(DB_ACL_CLIENT, true));
   }
   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

/*
 * List plugin objects types
 */
void BDB::bdb_list_plugin_object_types(JCR *jcr, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   Mmsg(cmd, "SELECT DISTINCT ObjectType FROM Object ORDER BY ObjectType ASC");
   bdb_lock();


   if (!QueryDB(jcr, cmd)) {
      Jmsg(jcr, M_ERROR, 0, _("Query %s failed!\n"), cmd);
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

/*
 * List plugin objects (search is based on object provided)
 */
void BDB::bdb_list_plugin_objects(JCR *jcr, OBJECT_DBR *obj_r, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM esc(PM_MESSAGE), tmp(PM_MESSAGE), where(PM_MESSAGE), join(PM_MESSAGE);

   bdb_lock();

   //TODO add ACL part
   obj_r->create_db_filter(jcr, where.handle());

   if (obj_r->ClientName[0] != 0) {
      bdb_escape_string(jcr, esc.c_str(), obj_r->ClientName, strlen(obj_r->ClientName));
      Mmsg(tmp, " Client.Name='%s'", esc.c_str());
      append_filter(where.handle(), tmp.c_str());
      Mmsg(join, " INNER JOIN Job On Object.JobId=Job.JobId "   
                 " INNER JOIN Client ON Job.ClientId=Client.ClientId ");
   }


   Mmsg(tmp, " ORDER BY ObjectId %s ", obj_r->order ? "DESC" : "ASC");
   pm_strcat(where, tmp.c_str());

   if (obj_r->limit) {
      Mmsg(tmp, " LIMIT %d ", obj_r->limit);
      pm_strcat(where, tmp.c_str());
   }

   switch (type) {
   case JSON_LIST:
   case VERT_LIST:
         Mmsg(cmd,
              "SELECT Object.ObjectId, Object.JobId, Object.Path, Object.Filename, Object.PluginName, Object.ObjectCategory, "
                    "Object.ObjectType, Object.ObjectName, Object.ObjectSource, "
                    "Object.ObjectUUID, Object.ObjectSize, Object.ObjectStatus, Object.ObjectCount "
            "FROM Object %s %s", join.c_str(), where.c_str());
         break;
   case HORZ_LIST:
         Mmsg(cmd,
            "SELECT Object.ObjectId, Object.JobId, Object.ObjectCategory, "
                    "Object.ObjectType, Object.ObjectName, Object.ObjectStatus "
            "FROM Object %s %s", join.c_str(), where.c_str());
         break;
   default:
         break;
   }

   if (!QueryDB(jcr, cmd)) {
      Jmsg(jcr, M_ERROR, 0, _("Query %s failed!\n"), cmd);
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

/*
 * List plugin objects from list of ids provided
 */
void BDB::bdb_list_plugin_objects_ids(JCR *jcr, char* id_list, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM msg;

   switch (type) {
   case JSON_LIST:
   case VERT_LIST:
         Mmsg(cmd,
            "SELECT Object.ObjectId, Object.JobId, Object.Path, Object.Filename, Object.PluginName, Object.ObjectCategory, "
                    "Object.ObjectType, Object.ObjectName, Object.ObjectSource, Object.ObjectUUID, "
                    "Object.ObjectSize, Object.ObjectStatus, Object.ObjectCount "
            "FROM Object WHERE ObjectId IN (%s) ORDER BY ObjectId ASC", id_list);
         break;
   case HORZ_LIST:
         Mmsg(cmd,
            "SELECT Object.ObjectId, Object.JobId, Object.ObjectCategory, "
                    "Object.ObjectType, Object.ObjectName, Object.ObjectUUID, Object.ObjectStatus "
            "FROM Object WHERE ObjectId IN (%s) ORDER BY ObjectId ASC", id_list);
         break;
   default:
         break;
   }

   bdb_lock();

   if (!QueryDB(jcr, cmd)) {
      Jmsg(jcr, M_ERROR, 0, _("Query %s failed!\n"), cmd);
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}
/*
 * List restore objects
 *
 * JobId | JobIds: List RestoreObjects for specific Job(s)
 * It is possible to specify the ObjectType using FileType field.
 */
void BDB::bdb_list_restore_objects(JCR *jcr, ROBJECT_DBR *rr, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM filter;
   char  ed1[50];
   char *jobid;

   /* The ACL checking is done on the bconsole command */
   if (rr->JobIds && is_a_number_list(rr->JobIds)) {
      jobid = rr->JobIds;

   } else if (rr->JobId) {
      jobid = edit_int64(rr->JobId, ed1);

   } else {
      return;
   }

   if (rr->FileType > 0) {
      Mmsg(filter, "AND ObjectType = %d ", rr->FileType);
   }

   bdb_lock();
   if (type == VERT_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT JobId, RestoreObjectId, ObjectName, "
           "PluginName, ObjectType "
           "FROM RestoreObject JOIN Job USING (JobId) WHERE JobId IN (%s) %s "
           "ORDER BY JobTDate ASC, RestoreObjectId",
           jobid, filter.c_str());
   } else {
      Mmsg(cmd, "SELECT JobId, RestoreObjectId, ObjectName, "
           "PluginName, ObjectType, ObjectLength "
           "FROM RestoreObject JOIN Job USING (JobId) WHERE JobId IN (%s) %s "
           "ORDER BY JobTDate ASC, RestoreObjectId",
           jobid, filter.c_str());
   }

   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

/*
 * If VolumeName is non-zero, list the record for that Volume
 */
bool BDB::bdb_search_media_records(JCR *jcr, MEDIA_DBR *mdbr,
                                   DB_RESULT_HANDLER *handler, void *ctx)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   if (mdbr->VolumeName[0] == 0) {
      return false;
   }

   bdb_lock();
   bdb_escape_string(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));
   const char *where = get_acl(DB_ACL_POOL, false);
   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_POOL)) : "";

   if (mdbr->limit == 0) {
      mdbr->limit = 50;
   }

   Mmsg(cmd, "SELECT VolumeName FROM Media %s WHERE Media.VolumeName ILIKE '%%%s%%' %s LIMIT %u",
        join,
        esc,
        where,
        mdbr->limit);

   if (!bdb_sql_query(cmd, handler, ctx)) {
      bdb_unlock();
      return false;
   }

   sql_free_result();
   bdb_unlock();
   return true;
}

/*
 * If VolumeName is non-zero, list the record for that Volume
 *   otherwise, list the Volumes in the Pool specified by PoolId
 */
void BDB::bdb_list_media_records(JCR *jcr, MEDIA_DBR *mdbr,
                      DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   const char *expiresin = expires_in[bdb_get_type_index()];

   bdb_lock();
   bdb_escape_string(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));
   const char *where = get_acl(DB_ACL_POOL, false);
   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_POOL)) : "";

   if (type == VERT_LIST || type == JSON_LIST) {
      if (mdbr->VolumeName[0] != 0) {

         Mmsg(cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
            "MediaType,MediaTypeId,FirstWritten,LastWritten,LabelDate,VolJobs,"
            "VolFiles,VolBlocks,VolParts,VolCloudParts,Media.CacheRetention,VolMounts,VolBytes,"
            "VolABytes,VolAPadding,"
            "VolHoleBytes,VolHoles,LastPartBytes,VolErrors,VolWrites,"
            "VolCapacityBytes,VolStatus,Media.Enabled,Media.Recycle,Media.VolRetention,"
            "Media.VolUseDuration,Media.MaxVolJobs,Media.MaxVolFiles,Media.MaxVolBytes,InChanger,"
            "EndFile,EndBlock,VolType,Media.LabelType,StorageId,DeviceId,"
            "MediaAddressing,VolReadTime,VolWriteTime,"
            "LocationId,RecycleCount,InitialWrite,Media.ScratchPoolId,Media.RecyclePoolId, "
            "Media.ActionOnPurge,%s AS ExpiresIn, Comment"
           " FROM Media %s WHERE Media.VolumeName='%s' %s",
              expiresin,
              join,
              esc,
              where
            );
      } else {
         Mmsg(cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
            "MediaType,MediaTypeId,FirstWritten,LastWritten,LabelDate,VolJobs,"
            "VolFiles,VolBlocks,VolParts,VolCloudParts,Media.CacheRetention,VolMounts,VolBytes,"
            "VolABytes,VolAPadding,"
            "VolHoleBytes,VolHoles,LastPartBytes,VolErrors,VolWrites,"
            "VolCapacityBytes,VolStatus,Media.Enabled,Media.Recycle,Media.VolRetention,"
            "Media.VolUseDuration,Media.MaxVolJobs,Media.MaxVolFiles,Media.MaxVolBytes,InChanger,"
            "EndFile,EndBlock,VolType,Media.LabelType,StorageId,DeviceId,"
            "MediaAddressing,VolReadTime,VolWriteTime,"
            "LocationId,RecycleCount,InitialWrite,Media.ScratchPoolId,Media.RecyclePoolId, "
            "Media.ActionOnPurge,%s AS ExpiresIn, Comment"
            " FROM Media %s WHERE Media.PoolId=%s %s ORDER BY MediaId",
              expiresin,
              join,
              edit_int64(mdbr->PoolId, ed1),
              where
            );
      }
   } else {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(cmd, "SELECT MediaId,VolumeName,VolStatus,Media.Enabled,"
            "VolBytes,VolFiles,Media.VolRetention,Media.Recycle,Slot,InChanger,MediaType,VolType,"
              "VolParts,%s AS ExpiresIn "
              "FROM Media %s WHERE Media.VolumeName='%s' %s",
              expiresin,
              join,
              esc,
              where
            );
      } else {
         Mmsg(cmd, "SELECT MediaId,VolumeName,VolStatus,Media.Enabled,"
            "VolBytes,VolFiles,Media.VolRetention,Media.Recycle,Slot,InChanger,MediaType,VolType,"
            "VolParts,LastWritten,%s AS ExpiresIn "
            "FROM Media %s WHERE Media.PoolId=%s %s ORDER BY MediaId",
              expiresin,
              join,
              edit_int64(mdbr->PoolId, ed1),
              where
            );
      }
   }
   Dmsg1(DT_SQL|50, "q=%s\n", cmd);
   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

void BDB::bdb_list_jobmedia_records(JCR *jcr, uint32_t JobId, char *volume,
                              DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM where2;
   bdb_lock();
   /* Get some extra SQL parameters if needed */
   const char *where = get_acls(DB_ACL_BIT(DB_ACL_JOB)     |
                                DB_ACL_BIT(DB_ACL_FILESET) |
                                DB_ACL_BIT(DB_ACL_CLIENT), (JobId == 0 || volume != NULL));

   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_JOB)     |
                                                   DB_ACL_BIT(DB_ACL_FILESET) |
                                                   DB_ACL_BIT(DB_ACL_CLIENT)) : "";

   if (JobId) {
      Mmsg(where2, " WHERE JobMedia.JobId=%lu ", JobId);
   }
   
   if (volume) {
      POOL_MEM tmp, tmp2;
      int len = strlen(volume);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), volume, len);
      Mmsg(tmp2, " %s Media.VolumeName = '%s' ", JobId == 0 ?"WHERE": "AND", tmp.c_str());
      pm_strcat(where2, tmp2.c_str());
   }

   if (type == VERT_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
           "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
           "JobMedia.EndBlock "
           "FROM JobMedia JOIN Media USING (MediaId) %s "
           "%s %s ORDER BY JobMediaId ASC",
           join,
           where2.c_str(),
           where);

   } else {
      Mmsg(cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
           "FROM JobMedia JOIN Media USING (MediaId) %s %s %s ORDER BY JobMediaId ASC",
           join,
           where2.c_str(),
           where);
   }
   Dmsg1(DT_SQL|50, "q=%s\n", cmd);

   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}

/* List FileMedia records for a given job/file */
void BDB::bdb_list_filemedia_records(JCR *jcr, uint32_t JobId, uint32_t FileIndex,
                                     DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM tmp, filter;
   char ed1[50];

   if (JobId > 0) {
      Mmsg(filter, "AND FileMedia.JobId=%s ", edit_int64(JobId, ed1));
   }
   if (FileIndex > 0) {
      Mmsg(tmp, "AND FileMedia.FileIndex=%s ", edit_int64(FileIndex, ed1));
      pm_strcat(filter, tmp.c_str());
   }
   
   bdb_lock();
   if (type == VERT_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT JobId,FileIndex,Media.MediaId,Media.VolumeName,"
           "BlockAddress,RecordNo,FileOffset "
           "FROM FileMedia,Media WHERE Media.MediaId=FileMedia.MediaId "
           "%s ORDER BY FileIndex ASC, FileOffset ASC", filter.c_str());
   } else {
      Mmsg(cmd, "SELECT JobId,FileIndex,Media.VolumeName,BlockAddress,RecordNo,FileOffset "
           "FROM FileMedia,Media WHERE Media.MediaId=FileMedia.MediaId %s ORDER By FileIndex ASC, FileOffset ASC",
           filter.c_str());
   }
   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();
}


void BDB::bdb_list_copies_records(JCR *jcr, uint32_t limit, char *JobIds,
                            DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM str_limit(PM_MESSAGE);
   POOL_MEM str_jobids(PM_MESSAGE);

   if (limit > 0) {
      Mmsg(str_limit, " LIMIT %d", limit);
   }

   if (JobIds && JobIds[0]) {
      Mmsg(str_jobids, " AND (Job.PriorJobId IN (%s) OR Job.JobId IN (%s)) ",
           JobIds, JobIds);
   }

   bdb_lock();
   const char *where = get_acls(DB_ACL_BIT(DB_ACL_JOB) | DB_ACL_BIT(DB_ACL_CLIENT), false);
   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_CLIENT)) : "";

   Mmsg(cmd,
   "SELECT DISTINCT Job.PriorJobId AS JobId, Job.Job, "
                   "Job.JobId AS CopyJobId, Media.MediaType "
     "FROM Job %s "
     "JOIN JobMedia USING (JobId) "
     "JOIN Media    USING (MediaId) "
    "WHERE Job.Type = '%c' %s %s ORDER BY Job.PriorJobId DESC %s",
        join, (char) JT_JOB_COPY, where, str_jobids.c_str(), str_limit.c_str());

   if (!QueryDB(jcr, cmd)) {
      goto bail_out;
   }

   if (sql_num_rows()) {
      if (JobIds && JobIds[0]) {
         sendit(ctx, _("These JobIds have copies as follows:\n"));
      } else {
         sendit(ctx, _("The catalog contains copies as follows:\n"));
      }

      list_result(jcr, this, sendit, ctx, type);
   }

   sql_free_result();

bail_out:
   bdb_unlock();
}

void BDB::bdb_list_events_records(JCR *jcr, EVENTS_DBR *rec,
                                 DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   bool p_and=false;
   POOL_MEM str_limit(PM_MESSAGE);
   POOL_MEM where(PM_MESSAGE);
   POOL_MEM tmp2(PM_MESSAGE);
   POOL_MEM tmp(PM_MESSAGE);

   bdb_lock();
   if (rec->limit > 0) {
      Mmsg(str_limit, " LIMIT %d OFFSET %d", rec->limit, rec->offset);
   }
   if (rec->EventsType[0]) {
      int len = strlen(rec->EventsType);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->EventsType, len);
      Mmsg(tmp2, "%s Events.EventsType = '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (rec->EventsDaemon[0]) {
      int len = strlen(rec->EventsDaemon);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->EventsDaemon, len);
      Mmsg(tmp2, "%s Events.EventsDaemon = '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (rec->EventsSource[0]) {
      int len = strlen(rec->EventsSource);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->EventsSource, len);
      Mmsg(tmp2, "%s Events.EventsSource = '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (rec->EventsCode[0]) {
      int len = strlen(rec->EventsCode);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->EventsCode, len);
      Mmsg(tmp2, "%s Events.EventsCode = '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (rec->start[0]) {
      int len = strlen(rec->start);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->start, len);
      Mmsg(tmp2, "%s Events.EventsTime >= '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (rec->end[0]) {
      int len = strlen(rec->end);
      tmp.check_size(len*2+1);
      db_escape_string(jcr, this, tmp.c_str(), rec->end, len);
      Mmsg(tmp2, "%s Events.EventsTime <= '%s' ", p_and?"AND": "WHERE", tmp.c_str());
      pm_strcat(where, tmp2.c_str());
      p_and=true;
   }
   if (type == HORZ_LIST) {
      Mmsg(cmd,
   "SELECT EventsTime AS Time, EventsDaemon AS Daemon, EventsSource AS Source, EventsType AS Type, EventsText AS Events "
     "FROM Events "
    "%s ORDER BY Events.EventsTime %s %s",
        where.c_str(),
        rec->order ? "DESC" : "ASC",
        str_limit.c_str());

   } else if (type == JSON_LIST) {
      Mmsg(tmp2, to_unix_timestamp[bdb_get_type_index()], "EventsTime");
      Mmsg(cmd,
   "SELECT EventsTime AS Time, %s AS UnixTime, EventsCode AS Code, EventsDaemon AS Daemon, EventsRef AS Ref, EventsType AS Type, EventsSource AS Source, EventsText AS Events "
     "FROM Events "
    "%s ORDER BY Events.EventsTime %s %s",
        tmp2.c_str(),
        where.c_str(),
        rec->order ? "DESC" : "ASC",
        str_limit.c_str());

   } else {
      Mmsg(cmd,
   "SELECT EventsTime AS Time, EventsCode AS Code, EventsDaemon AS Daemon, EventsRef AS Ref, EventsType AS Type, EventsSource AS Source, EventsText AS Events "
     "FROM Events "
    "%s ORDER BY Events.EventsTime %s %s",
        where.c_str(),
        rec->order ? "DESC" : "ASC",
        str_limit.c_str());
   }
   if (!QueryDB(jcr, cmd)) {
      goto bail_out;
   }
   list_result(jcr, this, sendit, ctx, type);

bail_out:
   bdb_unlock();
}

void BDB::bdb_list_joblog_records(JCR *jcr, uint32_t JobId,
                              DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   char ed1[50];
 
   if (JobId <= 0) {
      return;
   }
   bdb_lock();

   const char *where = get_acls(DB_ACL_BIT(DB_ACL_JOB)     |
                                DB_ACL_BIT(DB_ACL_FILESET) |
                                DB_ACL_BIT(DB_ACL_CLIENT), false);

   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_JOB)     |
                                                   DB_ACL_BIT(DB_ACL_FILESET) |
                                                   DB_ACL_BIT(DB_ACL_CLIENT)) : "";

   if (type == VERT_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT Time,LogText FROM Log %s "
           "WHERE Log.JobId=%s %s ORDER BY LogId ASC",
           join,
           edit_int64(JobId, ed1),
           where);

   } else {
      Mmsg(cmd, "SELECT LogText FROM Log %s "
           "WHERE Log.JobId=%s %s ORDER BY LogId ASC",
           join,
           edit_int64(JobId, ed1),
           where);
   }
   Dmsg1(DT_SQL|50, "q=%s\n", cmd);
   if (!QueryDB(jcr, cmd)) {
      goto bail_out;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();

bail_out:
   bdb_unlock();
}

/*
 * List Job record(s) that match JOB_DBR
 *
 *  Currently, we return all jobs or if jr->JobId is set,
 *  only the job with the specified id.
 */
alist *BDB::bdb_list_job_records(JCR *jcr, JOB_DBR *jr, DB_LIST_HANDLER *sendit,
                    void *ctx, e_list_type type)
{
   char ed1[50];
   char limit[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   alist *list = NULL;
   POOLMEM *where  = get_pool_memory(PM_MESSAGE);
   POOLMEM *tmp    = get_pool_memory(PM_MESSAGE);
   const char *order = "ASC";
   const char *join = "";
   const char *where_tmp = "";
   *where = 0;

   bdb_lock();
   if (jr->order == 1) {
      order = "DESC";
   }
   if (jr->limit > 0) {
      snprintf(limit, sizeof(limit), " LIMIT %d", jr->limit);
   } else {
      limit[0] = 0;
   }
   if (jr->Name[0]) {
      bdb_escape_string(jcr, esc, jr->Name, strlen(jr->Name));
      Mmsg(tmp, " Job.Name='%s' ", esc);
      append_filter(&where, tmp);

   } else if (jr->JobId != 0) {
      Mmsg(tmp, " Job.JobId=%s ", edit_int64(jr->JobId, ed1));
      append_filter(&where, tmp);

   } else if (jr->Job[0] != 0) {
      bdb_escape_string(jcr, esc, jr->Job, strlen(jr->Job));
      Mmsg(tmp, " Job.Job='%s' ", esc);
      append_filter(&where, tmp);

   } else if (jr->Reviewed > 0) {
      Mmsg(tmp, " Job.Reviewed = %d ", jr->Reviewed);
      append_filter(&where, tmp);
   }

   if (type == INCOMPLETE_JOBS && jr->JobStatus == JS_FatalError) {
      Mmsg(tmp, " Job.JobStatus IN ('E', 'f') ");
      append_filter(&where, tmp);

   } else if (jr->JobStatus) {
      Mmsg(tmp, " Job.JobStatus='%c' ", jr->JobStatus);
      append_filter(&where, tmp);
   }

   if (jr->JobType) {
      Mmsg(tmp, " Job.Type='%c' ", jr->JobType);
      append_filter(&where, tmp);
   }

   if (jr->JobLevel) {
      Mmsg(tmp, " Job.Level='%c' ", jr->JobLevel);
      append_filter(&where, tmp);
   }

   if (jr->JobErrors > 0) {
      Mmsg(tmp, " Job.JobErrors > 0 ");
      append_filter(&where, tmp);
   }

   if (jr->ClientId > 0) {
      Mmsg(tmp, " Job.ClientId=%s ", edit_int64(jr->ClientId, ed1));
      append_filter(&where, tmp);
   }

   if (jr->FromDate[0]) {
      bdb_escape_string(jcr, esc, jr->FromDate, strlen(jr->FromDate));
      Mmsg(tmp, " Job.StartTime >= '%s' ", esc);
      append_filter(&where, tmp);
   }

   where_tmp = get_acls(DB_ACL_BIT(DB_ACL_CLIENT)  |
                        DB_ACL_BIT(DB_ACL_JOB)     |
                        DB_ACL_BIT(DB_ACL_FILESET),
                        where[0] == 0);
   pm_strcat(where, where_tmp);

   if (*where_tmp) {
      join = get_acl_join_filter(DB_ACL_BIT(DB_ACL_CLIENT)  |
                                 DB_ACL_BIT(DB_ACL_FILESET));
   }

   switch (type) {
   case JSON_LIST:
   case VERT_LIST:
      Mmsg(cmd,
           "SELECT JobId,Job,Job.Name,PurgedFiles,Type,Level,"
           "Job.ClientId,Client.Name as ClientName,JobStatus,Status.JobStatusLong,SchedTime,"
           "StartTime,EndTime,RealEndTime,JobTDate,"
           "VolSessionId,VolSessionTime,JobFiles,JobBytes,ReadBytes,JobErrors,"
           "JobMissingFiles,Job.PoolId,Pool.Name as PoolName,PriorJobId,"
           "Job.FileSetId,FileSet.FileSet,Job.HasCache,Comment,Reviewed "
           "FROM Job JOIN Client USING (ClientId) LEFT JOIN Pool USING (PoolId) "
           "LEFT JOIN FileSet USING (FileSetId) LEFT JOIN Status USING (JobStatus) %s "
           "ORDER BY StartTime %s %s", where, order, limit);
      break;
   case HORZ_LIST:
      Mmsg(cmd,
           "SELECT JobId,Job.Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
           "FROM Job %s %s ORDER BY StartTime %s,JobId %s %s", join, where, order, order, limit);
      break;
   case INCOMPLETE_JOBS:
      Mmsg(cmd,
           "SELECT JobId,Job.Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
             "FROM Job %s %s ORDER BY StartTime %s,JobId %s %s",
           join, where, order, order, limit);
      break;
   default:
      break;
   }
   Dmsg1(DT_SQL|50, "SQL: %s\n", cmd);

   free_pool_memory(tmp);
   free_pool_memory(where);

   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return NULL;
   }
   if (type == INCOMPLETE_JOBS) {
      SQL_ROW row;
      list = New(alist(10));
      sql_data_seek(0);
      for (int i=0; (row=sql_fetch_row()) != NULL; i++) {
         list->append(bstrdup(row[0]));
      }
   }
   sql_data_seek(0);
   list_result(jcr, this, sendit, ctx, type);
   sql_free_result();
   bdb_unlock();
   return list;
}

/*
 * List Job totals
 *
 */
void BDB::bdb_list_job_totals(JCR *jcr, JOB_DBR *jr, DB_LIST_HANDLER *sendit, void *ctx)
{
   bdb_lock();
   const char *where = get_acls(DB_ACL_BIT(DB_ACL_CLIENT) | DB_ACL_BIT(DB_ACL_JOB), true);
   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_CLIENT)) : "";

   /* List by Job */
   Mmsg(cmd, "SELECT  count(*) AS Jobs,sum(JobFiles) "
        "AS Files,sum(JobBytes) AS Bytes,Job.Name AS Job FROM Job %s %s GROUP BY Job.Name",
        join, where);

   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, HORZ_LIST);

   sql_free_result();

   /* Do Grand Total */
   Mmsg(cmd, "SELECT count(*) AS Jobs,sum(JobFiles) "
        "AS Files,sum(JobBytes) As Bytes FROM Job %s %s",
        join, where);

   if (!QueryDB(jcr, cmd)) {
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, HORZ_LIST);

   sql_free_result();
   bdb_unlock();
}

/* List all file records from a job
 * "deleted" values are described just below
 */
void BDB::bdb_list_files_for_job(JCR *jcr, JobId_t jobid, int deleted, DB_LIST_HANDLER *sendit, void *ctx)
{
   char ed1[50];
   const char *opt;
   LIST_CTX lctx(jcr, this, sendit, ctx, HORZ_LIST);

   switch (deleted) {
   case 0:                      /* Show only actual files */
      opt = " AND FileIndex > 0 ";
      break;
   case 1:                      /* Show only deleted files */
      opt = " AND FileIndex <= 0 ";
      break;
   default:                     /* Show everything */
      opt = "";
      break;
   }

   bdb_lock();
   /* Get optional filters for the SQL query */
   const char *where = get_acls(DB_ACL_BIT(DB_ACL_JOB) |
                                DB_ACL_BIT(DB_ACL_CLIENT) |
                                DB_ACL_BIT(DB_ACL_FILESET), true);

   const char *join = *where ? get_acl_join_filter(DB_ACL_BIT(DB_ACL_JOB) |
                                                   DB_ACL_BIT(DB_ACL_CLIENT) |
                                                   DB_ACL_BIT(DB_ACL_FILESET)) : "";

   /*
    * MySQL is different with no || operator
    */
   if (bdb_get_type_index() == SQL_TYPE_MYSQL) {
      Mmsg(cmd, "SELECT CONCAT(Path.Path,F.Filename) AS Filename "
           "FROM (SELECT PathId, Filename, JobId FROM File WHERE JobId=%s %s"
                  "UNION ALL "
                 "SELECT PathId, Filename, BaseFiles.JobId  "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F JOIN Path ON (Path.PathId=F.PathId) %s %s",
           edit_int64(jobid, ed1), opt, ed1, join, where);
   } else {
      Mmsg(cmd, "SELECT Path.Path||F.Filename AS Filename "
           "FROM (SELECT PathId, Filename, JobId FROM File WHERE JobId=%s %s"
                  "UNION ALL "
                 "SELECT PathId, Filename, BaseFiles.JobId "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F JOIN Path ON (Path.PathId=F.PathId) %s %s",
           edit_int64(jobid, ed1), opt, ed1, join, where);
   }
   Dmsg1(DT_SQL|50, "q=%s\n", cmd);
   if (!bdb_big_sql_query(cmd, list_result, &lctx)) {
       bdb_unlock();
       return;
   }

   lctx.send_dashes();

   sql_free_result();
   bdb_unlock();
}

void BDB::bdb_list_base_files_for_job(JCR *jcr, JobId_t jobid, DB_LIST_HANDLER *sendit, void *ctx)
{
   char ed1[50];
   LIST_CTX lctx(jcr, this, sendit, ctx, HORZ_LIST);

   bdb_lock();

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (bdb_get_type_index() == SQL_TYPE_MYSQL) {
      Mmsg(cmd, "SELECT CONCAT(Path.Path,File.Filename) AS Filename "
           "FROM BaseFiles, File, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Path.PathId=File.PathId",
         edit_int64(jobid, ed1));
   } else {
      Mmsg(cmd, "SELECT Path.Path||File.Filename AS Filename "
           "FROM BaseFiles, File, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Path.PathId=File.PathId",
           edit_int64(jobid, ed1));
   }

   if (!bdb_big_sql_query(cmd, list_result, &lctx)) {
       bdb_unlock();
       return;
   }

   lctx.send_dashes();

   sql_free_result();
   bdb_unlock();
}

void BDB::bdb_list_snapshot_records(JCR *jcr, SNAPSHOT_DBR *sdbr,
              DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOLMEM *filter = get_pool_memory(PM_MESSAGE);
   POOLMEM *tmp    = get_pool_memory(PM_MESSAGE);
   POOLMEM *esc    = get_pool_memory(PM_MESSAGE);
   char ed1[50];

   bdb_lock();
   const char *where = get_acl(DB_ACL_CLIENT, false);

   *filter = 0;
   if (sdbr->Name[0]) {
      bdb_escape_string(jcr, esc, sdbr->Name, strlen(sdbr->Name));
      Mmsg(tmp, "Name='%s'", esc);
      append_filter(&filter, tmp);
   }
   if (sdbr->SnapshotId > 0) {
      Mmsg(tmp, "Snapshot.SnapshotId=%d", sdbr->SnapshotId);
      append_filter(&filter, tmp);
   }
   if (sdbr->ClientId > 0) {
      Mmsg(tmp, "Snapshot.ClientId=%d", sdbr->ClientId);
      append_filter(&filter, tmp);
   }
   if (sdbr->JobId > 0) {
      Mmsg(tmp, "Snapshot.JobId=%d", sdbr->JobId);
      append_filter(&filter, tmp);
   }
   if (*sdbr->Client) {
      bdb_escape_string(jcr, esc, sdbr->Client, strlen(sdbr->Client));
      Mmsg(tmp, "Client.Name='%s'", esc);
      append_filter(&filter, tmp);
   }
   if (sdbr->Device && *(sdbr->Device)) {
      esc = check_pool_memory_size(esc, strlen(sdbr->Device) * 2 + 1);
      bdb_escape_string(jcr, esc, sdbr->Device, strlen(sdbr->Device));
      Mmsg(tmp, "Device='%s'", esc);
      append_filter(&filter, tmp);
   }
   if (*sdbr->Type) {
      bdb_escape_string(jcr, esc, sdbr->Type, strlen(sdbr->Type));
      Mmsg(tmp, "Type='%s'", esc);
      append_filter(&filter, tmp);
   }
   if (*sdbr->created_before) {
      bdb_escape_string(jcr, esc, sdbr->created_before, strlen(sdbr->created_before));
      Mmsg(tmp, "CreateDate <= '%s'", esc);
      append_filter(&filter, tmp);
   }
   if (*sdbr->created_after) {
      bdb_escape_string(jcr, esc, sdbr->created_after, strlen(sdbr->created_after));
      Mmsg(tmp, "CreateDate >= '%s'", esc);
      append_filter(&filter, tmp);
   }
   if (sdbr->expired) {
      Mmsg(tmp, "CreateTDate < (%s - Retention)", edit_int64(time(NULL), ed1));
      append_filter(&filter, tmp);
   }
   if (*sdbr->CreateDate) {
      bdb_escape_string(jcr, esc, sdbr->CreateDate, strlen(sdbr->CreateDate));
      Mmsg(tmp, "CreateDate = '%s'", esc);
      append_filter(&filter, tmp);
   }

   if (sdbr->sorted_client) {
      pm_strcat(filter, " ORDER BY Client.Name, SnapshotId DESC");

   } else {
      pm_strcat(filter, " ORDER BY SnapshotId DESC");
   }

   if (type == VERT_LIST || type == ARG_LIST || type == JSON_LIST) {
      Mmsg(cmd, "SELECT SnapshotId, Snapshot.Name, CreateDate, Client.Name AS Client, "
           "FileSet.FileSet AS FileSet, JobId, Volume, Device, Type, Retention, Comment "
           "FROM Snapshot JOIN Client USING (ClientId) LEFT JOIN FileSet USING (FileSetId) %s %s", filter, where);

   } else if (type == HORZ_LIST) {
      Mmsg(cmd, "SELECT SnapshotId, Snapshot.Name, CreateDate, Client.Name AS Client, "
           "Device, Type "
           "FROM Snapshot JOIN Client USING (ClientId) %s", filter, where);
   }

   if (!QueryDB(jcr, cmd)) {
      goto bail_out;
   }

   list_result(jcr, this, sendit, ctx, type);

bail_out:
   sql_free_result();
   bdb_unlock();

   free_pool_memory(filter);
   free_pool_memory(esc);
   free_pool_memory(tmp);
}

void BDB::bdb_list_files(JCR *jcr, FILE_DBR *fr, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   uint32_t firstindex = fr->FileIndex;
   uint32_t lastindex = fr->FileIndex2 ? fr->FileIndex2 : fr->FileIndex;

   bdb_lock();

   Mmsg(cmd, "SELECT Path.Path, File.Filename, File.FileIndex, File.JobId, "
        "File.LStat, File.DeltaSeq, File.Md5 "
        "FROM File JOIN Path USING (PathId) "
        "WHERE FileIndex >= %ld AND FileIndex <= %ld AND JobId = %ld",
        firstindex, lastindex, fr->JobId);

   if (!bdb_sql_query(cmd, result_handler, ctx)) {
      goto bail_out;
   }

bail_out:
   bdb_unlock();
}

void BDB::bdb_list_tag_records(JCR *jcr, TAG_DBR *tag, DB_LIST_HANDLER *result_handler, void *ctx, e_list_type type)
{
   POOL_MEM tmp, where;
   char esc[MAX_ESCAPE_NAME_LENGTH];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   uint64_t aclbits, aclbits_extra;
   
   const char *name;
   const char *id;
   const char *table;

   tag->gen_sql(jcr, this, &table, &name, &id, esc, esc_name, &aclbits, &aclbits_extra);

   bdb_lock();
   pm_strcpy(where, get_acls(aclbits, true)); /* get_acls() uses a specific object buffer */
   const char *whereand = get_acls(aclbits, false);
   const char *join = get_acl_join_filter(aclbits_extra);

   if (table) {
      if (tag->all) {
         if (*esc_name) {
            /* Display all resource for a tag */
            Mmsg(tmp, "SELECT %s, %s AS %s FROM Tag%s AS T JOIN %s USING (%s) %s WHERE T.Tag = '%s' %s",
                 id, name, table, table, table, id, join, esc_name, whereand);
         } else {
            /* Display all tags for a resource type */
            Mmsg(tmp, "SELECT DISTINCT T.Tag, %s AS %s, %s AS %s FROM Tag%s AS T JOIN %s USING (%s) %s %s",
                 id, id, name, table, table, table, id, join, where.c_str());
         }

      } else {
         if (*esc_name) {
            Mmsg(tmp, "SELECT T.Tag, %s as %s, %s AS %s FROM Tag%s AS T JOIN %s USING (%s) %s WHERE %s = '%s' AND T.Tag = '%s' %s",
                 id, id, name, table, table, table, id, join, name, esc, esc_name, whereand);

         } else {
            /* Display all tags for a client */
            Mmsg(tmp, "SELECT Tag, %s as %s, %s as %s FROM Tag%s AS T JOIN %s USING (%s) %s WHERE %s = '%s' %s",
                 id, id, (tag->JobId>0)?"Name":name, table, table, table, id, join, name, esc, whereand);
         }
      }
      Dmsg1(DT_SQL|50, "q=%s\n", tmp.c_str());
      bdb_list_sql_query(jcr, tmp.c_str(), result_handler, ctx, 0, type);
   }
   bdb_unlock();
}

/*
 * List plugin objects (search is based on object provided)
 */
void BDB::bdb_list_metadata_records(JCR *jcr, META_DBR *meta_r, DB_LIST_HANDLER *sendit, void *ctx, e_list_type type)
{
   POOL_MEM esc(PM_MESSAGE), tmp(PM_MESSAGE), where(PM_MESSAGE), join(PM_MESSAGE);
   bdb_lock();
   //TODO add ACL part
   meta_r->create_db_filter(jcr, this, where.handle());
   Dmsg1(DT_SQL|50, "where=[%s]\n", where.c_str());

   const char *where_filter = get_acls(DB_ACL_BIT(DB_ACL_JOB)     |
                                       DB_ACL_BIT(DB_ACL_CLIENT), strcmp(where.c_str(), "") == 0);

   const char *join_filter = (*where_filter && meta_r->ClientName[0] == 0) ?
      get_acl_join_filter(DB_ACL_BIT(DB_ACL_CLIENT)) : "";

   if (meta_r->ClientName[0] != 0) {
      bdb_escape_string(jcr, esc.c_str(), meta_r->ClientName, strlen(meta_r->ClientName));
      Mmsg(tmp, " Client.Name='%s'", esc.c_str());
      append_filter(where.handle(), tmp.c_str());
      Mmsg(join, " JOIN Job ON (Job.JobId = Meta%s.JobId) JOIN Client USING (ClientId) ", meta_r->Type);

   } else if (*where_filter) {  // We add manually the Job join filter part
      Mmsg(join, " JOIN Job ON (Job.JobId = Meta%s.JobId) ", meta_r->Type);
   }

   if (strcmp(meta_r->Type, "Attachment") == 0) {
      pm_strcat(join, " JOIN MetaEmail ON (EmailId = AttachmentEmailId AND MetaEmail.JobId = MetaAttachment.JobId) ");
   }
   
   if (where_filter && *where_filter) {
      pm_strcat(where, where_filter);
   }

   if (join_filter && *join_filter) {
      pm_strcat(join, join_filter);
   }
   
   if (meta_r->orderby == 1) {
      Mmsg(tmp, " ORDER BY EmailTime %s ", meta_r->order ? "DESC" : "ASC");
   } else {
      Mmsg(tmp, " ORDER BY Meta%s.JobId, Meta%s.FileIndex %s ",
           meta_r->Type, meta_r->Type, meta_r->order ? "DESC" : "ASC");
   }

   pm_strcat(where, tmp.c_str());

   if (meta_r->limit > 0) {
      Mmsg(tmp, " LIMIT %d ", meta_r->limit);
      pm_strcat(where, tmp.c_str());
   }

   if (meta_r->offset > 0) {
      Mmsg(tmp, " OFFSET %ld ", meta_r->offset);
      pm_strcat(where, tmp.c_str());
   }


   switch (type) {
   case JSON_LIST:
   case VERT_LIST:
      meta_r->get_all_keys(tmp.handle());
      Mmsg(cmd,
           "SELECT %s %s "
           "FROM Meta%s %s %s",
           (strcmp(meta_r->Type, "Email") == 0) ? "" : "DISTINCT",
           tmp.c_str(), meta_r->Type, join.c_str(), where.c_str());
         break;
   case HORZ_LIST:
      meta_r->get_important_keys(tmp.handle());
         Mmsg(cmd,
            "SELECT %s %s "
              "FROM Meta%s %s %s",
              (strcmp(meta_r->Type, "Email") == 0) ? "" : "DISTINCT",
              tmp.c_str(), meta_r->Type, join.c_str(), where.c_str());
         break;
   default:
         break;
   }
   Dmsg1(DT_SQL|50, "%s\n", cmd);
   if (!QueryDB(jcr, cmd)) {
      Jmsg(jcr, M_ERROR, 0, _("Query %s failed!\n"), cmd);
      bdb_unlock();
      return;
   }

   list_result(jcr, this, sendit, ctx, type);

   sql_free_result();
   bdb_unlock();

}


#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL */
