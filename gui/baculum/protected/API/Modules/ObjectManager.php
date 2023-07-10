<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2022 Kern Sibbald
 *
 * The main author of Baculum is Marcin Haba.
 * The original author of Bacula is Kern Sibbald, with contributions
 * from many others, a complete list can be found in the file AUTHORS.
 *
 * You may use this file and others of this release according to the
 * license defined in the LICENSE file, which includes the Affero General
 * Public License, v3.0 ("AGPLv3") and some additional permissions and
 * terms pursuant to its AGPLv3 Section 7.
 *
 * This notice must be preserved when any source code is
 * conveyed and/or propagated.
 *
 * Bacula(R) is a registered trademark of Kern Sibbald.
 */

namespace Baculum\API\Modules;

use Baculum\Common\Modules\Logging;
use PDO;

/**
 * Object manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class ObjectManager extends APIModule
{

	/**
	 * Allowed order columns for object overview.
	 */
	public static $overview_order_columns = [
		'object' => ['objectname', 'objectcategory', 'client', 'jobstatus', 'endtime'],
		'file' => ['client', 'jobstatus', 'endtime', 'fileset']
	];

	/**
	 * Object result in job and object endpoint can be displayed in on of the two views:
	 *  - basic - display only base job and object properties
	 *  - full - display all properties
	 * Here are object properties for basic view.
	 */
	public static $basic_mode_obj_props = [
		'ObjectId',
		'JobId',
		'ObjectCategory',
		'ObjectType',
		'ObjectName',
		'ObjectSource',
		'ObjectSize',
		'ObjectStatus',
		'ObjectCount'
	];

	/**
	 * Object result modes.
	 * Modes:
	 *  - normal - record list without any additional data
	 *  - overview - record list with some summary count (files, vSphere, MySQL, PostgreSQL...)
	 */
	const OBJECT_RESULT_MODE_NORMAL = 'normal';
	const OBJECT_RESULT_MODE_OVERVIEW = 'overview';

	/**
	 * Object result record view.
	 * Views:
	 *  - basic - list only limited record properties
	 *  - full - list all record properties
	 */
	const OBJ_RESULT_VIEW_BASIC = 'basic';
	const OBJ_RESULT_VIEW_FULL = 'full';

	/**
	 * Get objects.
	 *
	 * @param array $criteria SQL criteria in nested array format (@see  Databaes::getWhere)
	 * @param array $opts object options
	 * @param integer $limit_val maximum number of elements to return
	 * @param integer $offset_val query offset number
	 * @param string $sort_col column to sort
	 * @param string $sort_order sort order (asc - ascending, desc - descending)
	 * @param string $group_by column to group
	 * @param integer $group_limit maximum number of elements in one group
	 * @param string $view job records view (basic, full)
	 * @return array object list
	 */
	public function getObjects($criteria = [], $opts = [], $limit_val = null, $offset_val = 0, $sort_col = 'ObjectId', $sort_order = 'DESC', $group_by = null, $group_limit = 0, $group_offset = 0, $group_order_by = null, $group_order_direction = 'ASC', $view = self::OBJ_RESULT_VIEW_FULL, $mode = self::OBJECT_RESULT_MODE_NORMAL) {
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
		    $sort_col = strtolower($sort_col);
		}
		$order = sprintf(
			' ORDER BY %s %s',
			$sort_col,
			$sort_order
		);
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = sprintf(
				' LIMIT %d',
				$limit_val
			);
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = sprintf(
				' OFFSET %d',
				$offset_val
			);
		}

		$where = Database::getWhere($criteria);

		$obj_record = 'Object.*, Job.Name AS jobname, Job.StartTime AS starttime, Job.JobErrors AS joberrors, Job.JobStatus AS jobstatus, Client.Name AS client';
		if ($view == self::OBJ_RESULT_VIEW_BASIC) {
			$obj_record = implode(',', $this->basic_mode_obj_props);
		}
		$sql = 'SELECT ' . $obj_record . ' 
FROM Object 
JOIN Job USING (JobId) 
LEFT JOIN Client USING (ClientId) '
. $where['where'] . $order . $limit . $offset;
		$statement = Database::runQuery($sql, $where['params']);
		$result = $statement->fetchAll(\PDO::FETCH_OBJ);
		if (key_exists('unique_objects', $opts) && $opts['unique_objects']) {
			Database::groupBy('objectname', $result, 1, 0, 'objecttype');
			$func = function ($item) {
				return ((object)$item[0]);
			};
			$result = array_values($result);
			$result = array_map($func, $result);
		}
		$overview = Database::groupBy(
			$group_by,
			$result,
			$group_limit,
			$group_offset,
			'objecttype',
			$group_order_by,
			$group_order_direction
		);
		if ($mode == self::OBJECT_RESULT_MODE_OVERVIEW) {
			// Overview mode.
			$result = [
				'objects' => $result,
				'overview' => (is_string($group_by) ? $overview : $this->getObjectCountByObjectType($criteria))
			];
		}
		return $result;
	}

	/**
	 * Get object records in overview.
	 *
	 * @param array $general_criteria SQL criteria to get object list
	 * @param array $object_criteria SQL object specific criteria to get object list
	 * @param mixed $limit_val result limit value
	 * @param int $offset_val result offset value
	 * @param string $sort_col column to sort
	 * @param string $sort_order sort order (asc - ascending, desc - descending)
	 * @return array object record list or empty list if no object found
	 */
	public function getObjectsOverview($general_criteria = [], $object_criteria = [], $limit_val = null, $offset_val = 0, $sort_col = 'endtime', $sort_order = 'DESC') {
		$connection = ObjectRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();

		$limit = is_int($limit_val) && $limit_val > 0 ? ' LIMIT ' . $limit_val : '';
		$offset = is_int($offset_val) && $offset_val > 0 ? ' OFFSET ' . $offset_val : '';
		$sort_col_i = strtolower($sort_col);
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
		    $sort_col = $sort_col_i;
		}

		// default sorting for objects
		$obj_order = ' ORDER BY
			 ObjectType,
			 ObjectName,
			 ObjectUUID,
			 Client.Name,
			 Job.Name,
			 JobTDate DESC ';
		$file_order = ' ORDER BY
			FileSet.FileSet,
			Job.Name,
			Client.Name,
			JobTDate DESC ';

		if (empty($sort_col)) {
			$sort_col = 'JobTDate';
		} elseif (in_array($sort_col_i, ObjectManager::$overview_order_columns['object'])) {
			$obj_order .= sprintf(',%s %s', $sort_col, $sort_order);
		} elseif (in_array($sort_col_i, ObjectManager::$overview_order_columns['file'])) {
			$file_order .= sprintf(',%s %s', $sort_col, $sort_order);
		}
		$order = sprintf(
			' %s %s ',
			$sort_col,
			$sort_order
		);

		$result = [
			'overview' => []
		];
		$general_where = Database::getWhere($general_criteria, true);
		$object_crit_all = array_merge($general_criteria, $object_criteria);
		$object_where = Database::getWhere($object_crit_all);
		try {
			// start transaction
			$pdo->beginTransaction();

			// temporary table name
			$objects_tname1 = 'objects_1_' . getmypid();
			$objects_tname2 = 'objects_2_' . getmypid();
			$objects_tname3 = 'objects_3_' . getmypid();
			$objects_tname4 = 'objects_4_' . getmypid();

			$db_params = $this->getModule('api_config')->getConfig('db');
			if ($db_params['type'] === Database::PGSQL_TYPE) {
				// PostgreSQL block

				// create temporary table
				$sql = 'CREATE TEMPORARY TABLE ' . $objects_tname1 . ' AS
				SELECT DISTINCT ON (
					ObjectType,
					ObjectName,
					ObjectUUID,
					Client.Name,
					Job.Name
				)
					ObjectType      AS objecttype,
					ObjectName      AS objectname,
					Client.ClientId AS clienid,
					Client.Name     AS client,
					Job.Name        AS job,
					Job.Type	AS type,
					Job.Level       AS level,
					ObjectId        AS objectid,
					Object.JobId    AS jobid,
					JobTDate        AS jobtdate,
					Job.StartTime   AS starttime,
					Job.EndTime     AS endtime,
					ObjectCategory  AS objectcategory,
					ObjectStatus    AS objectstatus,
					ObjectSource    AS objectsource,
					ObjectSize      AS objectsize,
					Path            AS path,
					Job.JobStatus   AS jobstatus
				FROM Object
				JOIN Job USING (JobId)
				JOIN Client USING (ClientId)
				' . $object_where['where'] . $obj_order;

				Database::execute($sql, $object_where['params']);

				// get content for files
				$sql = 'CREATE TEMPORARY TABLE ' . $objects_tname4 . ' AS
						SELECT DISTINCT ON (FileSet.FileSet, Job.Name, Client.Name)
							FileSet.FileSet AS fileset,
							Job.Name        AS job,
							Client.ClientId AS clienid,
							Client.Name     AS client,
							JobId           AS jobid,
							Job.Type	AS type,
							Job.Level       AS level,
							JobTDate        AS jobtdate,
							StartTime       AS starttime,
							EndTime         AS endtime,
							JobStatus       AS jobstatus,
							JobBytes        AS jobbytes,
							JobFiles        AS jobfiles
						FROM Job
						JOIN FileSet USING (FileSetId)
						JOIN Client USING (ClientId)
						WHERE
							FileSet.Content = \'files\'
						' . (!empty($general_where['where']) ? ' AND ' . $general_where['where'] : '') . $file_order;

				Database::execute($sql, $general_where['params']);

			} elseif ($db_params['type'] === Database::MYSQL_TYPE) {
				// MySQL block

				// create temporary table 1 for objects
				$sql = 'CREATE TABLE ' . $objects_tname1 . ' AS
					SELECT CONCAT(
							ObjectType,
							ObjectName,
							ObjectUUID,
							Client.Name,
							Job.Name
						) AS K,
						ObjectType      AS objecttype,
						ObjectName      AS objectname,
						Client.ClientId AS clienid,
						Client.Name     AS client,
						Job.Name        AS Job,
						Job.Type	AS type,
						Job.Level       AS level,
						ObjectId        AS objectid,
						Object.JobId    AS jobid,
						JobTDate        AS jobtdate,
						StartTime       AS starttime,
						EndTime         AS endtime,
						ObjectCategory  AS objectcategory,
						ObjectStatus    AS objectstatus,
						ObjectSource    AS objectsource,
						ObjectSize      AS objectsize,
						Path            AS path,
						JobStatus       AS jobstatus
					FROM Object
					JOIN Job USING (JobId)
					JOIN Client USING (ClientId)' . $object_where['where'] . $obj_order;

				Database::execute($sql, $object_where['params']);

				// Create temporary table 2 for objects
				$sql = 'CREATE TEMPORARY TABLE ' . $objects_tname2 . ' AS
					SELECT ' . $objects_tname1 . '.K,
						objecttype,
						objectname,
						client,
						job,
						type,
						objectid,
						jobid,
						AAA.jobtdate,
						starttime,
						endtime,
						objectcategory,
						objectstatus,
						objectsource,
						objectsize,
						path,
						jobstatus
					FROM ' . $objects_tname1 . ' JOIN (
						SELECT
							AA.K,
							MAX(AA.jobtdate) AS jobtdate
						FROM ' . $objects_tname1 . ' AS AA
						GROUP BY AA.K
					) AS AAA ON (
						AAA.K = ' . $objects_tname1 . '.K AND AAA.jobtdate = ' . $objects_tname1 . '.jobtdate
					) ';

				Database::execute($sql);

				// Create temporary table 1 for files
				$sql = 'CREATE TABLE ' . $objects_tname3 . ' AS
					SELECT CONCAT(FileSet.FileSet, Job.Name, Client.Name) AS K,
						FileSet.FileSet AS fileset,
						Job.Name        AS job,
						Client.ClientId AS clienid,
						Client.Name     AS client,
						JobId           AS jobid,
						Job.Type	AS type,
						Job.Level       AS level,
						JobTDate        AS jobtdate,
						StartTime       AS starttime,
						EndTime         AS endtime,
						JobStatus       AS jobstatus,
						JobBytes        AS jobbytes,
						JobFiles        AS jobfiles
					FROM Job
					JOIN FileSet USING (FileSetId)
					JOIN Client USING (ClientId)
					WHERE
						FileSet.Content = \'files\'
					' . (!empty($general_where['where']) ? ' AND ' . $general_where['where'] : '') . $file_order;
				Database::execute($sql, $general_where['params']);

				// Create temporary table 2 for files
				$sql = 'CREATE TEMPORARY TABLE ' . $objects_tname4 . ' AS
					SELECT ' . $objects_tname3 . '.K,
						fileset,
						job,
						clienid,
						client,
						jobid,
						type,
						level,
						AAA.jobtdate,
						starttime,
						endtime,
						jobstatus,
						jobbytes,
						jobfiles
					FROM ' . $objects_tname3 . ' JOIN (
						SELECT
							AA.K,
							MAX(AA.jobtdate) AS jobtdate
						FROM ' . $objects_tname3 . ' AS AA
						GROUP BY AA.K
					) AS AAA ON (
						AAA.K = ' . $objects_tname3 . '.K AND AAA.jobtdate = ' . $objects_tname3 . '.jobtdate
					) ';
				Database::execute($sql);
			}

			// count for each type
			$sql = 'SELECT * FROM (
					SELECT
						ObjectType AS objecttype,
						COUNT(1)   AS count
					FROM ' . $objects_tname1 . '
					GROUP BY ObjectType
				) AS A UNION (
					SELECT
						\'files\' AS objecttype,
						COUNT(1)   AS count
					FROM ' . $objects_tname4 . '
					GROUP BY ObjectType
				)';
			$statement = Database::runQuery($sql);
			$object_count = $statement->fetchAll(PDO::FETCH_ASSOC);

			for ($i = 0; $i < count($object_count); $i++) {
				$items = [];
				if ($object_count[$i]['objecttype'] == 'files') {
					$sql = 'SELECT * 
						FROM ' . $objects_tname4 . '
						' . (in_array($sort_col_i, self::$overview_order_columns['file']) ? 'ORDER BY ' . $order : '') . $limit . $offset;
					$statement = Database::runQuery($sql);
					$items = $statement->fetchAll(PDO::FETCH_ASSOC);
				} else {
					$order_by = ['ObjectType', 'ObjectSource'];
					if ($sort_col_i !== 'objectcategory') {
						$order_by[] = 'ObjectCategory';
					}
					$sql = 'SELECT * 
						FROM ' . $objects_tname1 . '
						WHERE ObjectType = \'' . $object_count[$i]['objecttype'] . '\'
						ORDER BY ' . implode(',', $order_by)
						. (in_array($sort_col_i, self::$overview_order_columns['object']) ? ',' . $order : '') . $limit . $offset;
					$statement = Database::runQuery($sql);
					$items = $statement->fetchAll(PDO::FETCH_ASSOC);
				}
				if (!key_exists($object_count[$i]['objecttype'], $result['overview'])) {
					// type does not exists, add objects
					$result['overview'][$object_count[$i]['objecttype']] = [
						'items' => $items,
						'count' => $object_count[$i]['count']
					];
				} else {
					// type exists, update count only
					$result['overview'][$object_count[$i]['objecttype']]['count'] += $object_count[$i]['count'];
				}
			}

			// drop temporary tables
			$sql = 'DROP TABLE ' . $objects_tname1;
			Database::execute($sql);

			if ($db_params['type'] === Database::MYSQL_TYPE) {
				$sql = 'DROP TABLE ' . $objects_tname2;
				Database::execute($sql);
				$sql = 'DROP TABLE ' . $objects_tname3;
				Database::execute($sql);
				$sql = 'DROP TABLE ' . $objects_tname4;
				Database::execute($sql);
			}
		} catch(\PDOException $e) {
			// rollback the transaction
			$pdo->rollBack();

			// show the error message
			$msg = 'SQL transation commit error: ' . $e->getMessage();
			$this->getModule('logging')->log(
				Logging::CATEGORY_EXECUTE,
				$msg
			);
		}
		return $result;
	}

	public function getObjectById($objectid) {
		$params = [
			'Object.ObjectId' => [[
				'vals' => $objectid
			]]
		];
		$obj = $this->getObjects($params, [], 1);
		if (is_array($obj) && count($obj) > 0) {
			$obj = array_shift($obj);
		}
		return $obj;
	}

	/**
	 * Get object size statistics.
	 *
	 * @param string $objecttype object type (usually short name such as 'm365' or 'MySQL')
	 * @param string $objectsource object source
	 * @param string $datestart start date
	 * @param string $dateend end date
	 * @return array summary in form [sum => 0, month => '']
	 */
	public function getObjectSizeSum($objecttype = null, $objectsource = null, $datestart = null, $dateend = null) {
		$otype = '';
		if (!is_null($objecttype)) {
			$otype = ' AND Object.ObjectType=:objecttype ';

		}
		$osource = '';
		if (!is_null($objectsource)) {
			$osource = ' AND Object.ObjectSource=:objectsource ';
		}
		$dformat = 'Y-m-d H:i:s';
		if (is_null($datestart)) {
			$m_ago = new \DateTime('1 month ago');
			$datestart = $m_ago->format($dformat);
		}
		if (is_null($dateend)) {
			$dateend = date($dformat);
		}
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
			$date_month = ' date_trunc(\'month\', Job.StartTime) ';
		} elseif ($db_params['type'] === Database::MYSQL_TYPE) {
			$date_month = ' DATE_FORMAT(Job.StartTime, \'%Y-%m-01 00:00:00\') ';
		} elseif ($db_params['type'] === Database::SQLITE_TYPE) {
			$date_month = ' strftime(\'%Y-%m-01 00:00:00\', Job.StartTime) ';
		}
		$sql = 'SELECT SUM(Object.ObjectSize) AS sum,
			' . $date_month . '           AS month
			FROM Object
				LEFT JOIN Job USING (JobId)
			WHERE
				Job.StartTime BETWEEN :datestart AND :dateend
				' . $otype . $osource . '
			GROUP BY month
			ORDER BY month ASC';
		$connection = ObjectRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		if (!is_null($objecttype)) {
			$sth->bindParam(':objecttype', $objecttype, \PDO::PARAM_STR, 100);
		}
		if (!is_null($objectsource)) {
			$sth->bindParam(':objectsource', $objectsource, \PDO::PARAM_STR, 400);
		}
		$sth->bindParam(':datestart', $datestart, \PDO::PARAM_STR, 19);
		$sth->bindParam(':dateend', $dateend, \PDO::PARAM_STR, 19);
		$sth->execute();
		return $sth->fetchAll(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get all object versions by objectuuid.
	 *
	 * @param string $objectuuid object UUID
	 * @return array object versions
	 */
	public function getObjectVersions($objectuuid) {
		$sql = 'SELECT
				obj.ObjectUUID     AS objectuuid,
				obj.ObjectId	   AS objectid,
				obj.ObjectType	   AS objecttype,
				obj.ObjectName	   AS objectname,
				obj.ObjectCategory AS objectcategory,
				Job.JobId 	   AS jobid,
				Job.Name	   AS jobname,
				Job.Level	   AS level,
				Job.JobStatus	   AS jobstatus,
				Job.JobBytes	   AS jobbytes,
				Job.JobFiles	   AS jobfiles,
				Job.StartTime	   AS starttime,
				Job.EndTime	   AS endtime,
				Job.JobErrors	   AS joberrors,
				Client.Name	   AS client,
				FileSet.FileSet	   AS fileset
				FROM
					Object AS obj
					LEFT JOIN Job USING(JobId)
					LEFT JOIN Client USING(ClientId)
					LEFT JOIN FileSet USING(FileSetId)
				WHERE
					obj.ObjectUUID=:objectuuid
				ORDER BY Job.StartTime DESC';
		$connection = ObjectRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		$sth->bindParam(':objectuuid', $objectuuid, \PDO::PARAM_STR, 400);
		$sth->execute();
		return $sth->fetchAll(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get object job statistics by category.
	 * There are taken into account last three jobs.
	 *
	 * @param string $objecttype object type (usually short name such as 'm365' or 'MySQL')
	 * @param string $objectsource object source
	 * @param string $objectcategory object category like: 'mailbox' or 'team'
	 * @return array object statistics
	 */
	public function getObjectCategoryStatus($objecttype = null, $objectsource = null, $objectcategory = null) {
		$where = [];
		if (!is_null($objecttype)) {
			$where[] = ' Object.ObjectType=:objecttype ';
		}
		if (!is_null($objectsource)) {
			$where[] = ' Object.ObjectSource=:objectsource ';
		}
		if (!is_null($objectcategory)) {
			$where[] = ' Object.ObjectCategory=:objectcategory ';
		}
		$where_val = implode(' AND ', $where);

		$sql = 'SELECT
				DISTINCT b.JobId, b.JobStatus, b.StartTime, b.ObjectCategory
			FROM (
				SELECT
					Job.JobId	 AS jobid,
					Job.JobStatus	 AS jobstatus,
					Job.StartTime	 AS starttime,
					Object.ObjectCategory AS objectcategory,
					ROW_NUMBER() OVER (PARTITION BY Object.ObjectCategory ORDER BY Job.JobId DESC) AS r
				FROM Job
					JOIN Object USING(JobId)
				' . ($where_val ? ' WHERE ' . $where_val : '') . '
			) AS b
			WHERE
				b.r <= 3';
		$connection = ObjectRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		if (!is_null($objecttype)) {
			$sth->bindParam(':objecttype', $objecttype, \PDO::PARAM_STR, 100);
		}
		if (!is_null($objectsource)) {
			$sth->bindParam(':objectsource', $objectsource, \PDO::PARAM_STR, 400);
		}
		if (!is_null($objectcategory)) {
			$sth->bindParam(':objectcategory', $objectcategory, \PDO::PARAM_STR, 400);
		}
		$sth->execute();
		return $sth->fetchAll(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get total number of objects per object category.
	 *
	 * @param array $criteria SQL query criteria
	 * @return array object totals
	 */
	public function getObjectCategorySum($criteria) {
		$where = Database::getWhere($criteria);
		$wh = !empty($where['where']) ? $where['where'] : '';

		$sql = 'SELECT
			oobj.ObjectCategory AS objectcategory,
			oobj.ObjectType     AS objecttype,
			oobj.ObjectSource   AS objectsource,
			SUM(1)              AS count,
			MAX(Job.StartTime)  AS last_job_time
		FROM Object AS oobj
		JOIN Job USING(JobId)
			' . ($wh ? $wh . ' AND ' : ' WHERE ') . '
			oobj.JobId=(
				SELECT MAX(iobj.JobId) FROM Object AS iobj WHERE iobj.ObjectId=oobj.ObjectId
			)
		GROUP BY objectcategory, objecttype, objectsource';

		$connection = ObjectRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		$sth->execute($where['params']);
		return $sth->fetchAll(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get object count by object type.
	 * NOTE: It accepts the same criteria as ObjectManager::getObjects().
	 *
	 * @param array $criteria SQL criteria
	 * @return array object type counts
	 */
	public function getObjectCountByObjectType($criteria) {
		$where = Database::getWhere($criteria);
		$sql = 'SELECT DISTINCT ObjectType as objecttype,
					COUNT(1) AS count
			FROM Object
			JOIN Job USING (JobId)
			' . $where['where'] . '
			GROUP BY objecttype';
		$statement = Database::runQuery($sql, $where['params']);
		return $statement->fetchAll(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get existing object types.
	 *
	 * @return array object types
	 */
	public function getObjectTypes() {
		$sql = 'SELECT DISTINCT ObjectType as objecttype
			FROM Object
			ORDER BY ObjectType';
		$statement = Database::runQuery($sql);
		$result = $statement->fetchAll(\PDO::FETCH_GROUP);
		$values = array_keys($result);
		return $values;
	}

	/**
	 * Get existing object names.
	 *
	 * @param array $criteria SQL criteria in nested array format (@see  Databaes::getWhere)
	 * @param integer $limit_val maximum number of elements to return
	 * @return array object names
	 */
	public function getObjectNames($criteria = [], $limit_val = null) {
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = sprintf(
				' LIMIT %d',
				$limit_val
			);
		}
		$where = Database::getWhere($criteria);
		$sql = 'SELECT DISTINCT ObjectName as objectname
			FROM Object
			' . $where['where'] . ' 
			ORDER BY ObjectName ' . $limit;
		$statement = Database::runQuery($sql, $where['params']);
		$result = $statement->fetchAll(\PDO::FETCH_GROUP);
		$values = array_keys($result);
		return $values;
	}

	/**
	 * Get existing object categories.
	 *
	 * @param integer $limit_val maximum number of elements to return
	 * @return array object names
	 */
	public function getObjectCategories($limit_val = null) {
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = sprintf(
				' LIMIT %d',
				$limit_val
			);
		}
		$sql = 'SELECT DISTINCT ObjectCategory as objectcategory
			FROM Object
			ORDER BY ObjectCategory ' . $limit;
		$statement = Database::runQuery($sql);
		$result = $statement->fetchAll(PDO::FETCH_COLUMN);
		return $result;
	}
}
