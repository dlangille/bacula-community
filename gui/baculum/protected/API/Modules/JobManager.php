<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2020 Kern Sibbald
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
use Baculum\API\Modules\ObjectManager;
use PDO;
use Prado\Data\ActiveRecord\TActiveRecordCriteria;

/**
 * Job manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class JobManager extends APIModule {

	/**
	 * Job statuses in some parts are not compatible with rest of the API.
	 * NOTE: Used here are also internal job statuses that are not used in the Catalog
	 * but they are used internally by Bacula.
	 */
	private $js_successful = ['T'];
	private $js_unsuccessful = ['A', 'E', 'f'];
	private $js_warning = ['I', 'e', 'W'];
	private $js_running = ['C', 'B', 'D', 'F', 'L', 'M', 'R', 'S', 'a', 'c', 'd', 'i', 'j', 'l', 'm', 'p', 'q', 's', 't'];

	/**
	 * Job result in job and object endpoint can be displayed in on of the two views:
	 *  - basic - display only base job and object properties
	 *  - full - display all properties
	 * Here are job properties for basic view.
	 */
	private $basic_mode_job_props = [
		'Job.JobId',
		'Job.Job',
		'Job.Name',
		'Job.Type',
		'Job.Level',
		'Job.JobStatus',
		'Job.SchedTime',
		'Job.RealEndTime',
		'Job.JobFiles',
		'Job.JobBytes',
		'Job.JobErrors',
		'Job.Reviewed',
		'Job.Comment',
		'Job.RealStartTime',
		'Job.IsVirtualFull',
		'Job.CompressRatio',
		'Job.Rate',
		'Job.StatusInfo',
		'Job.Encrypted'
	];

	/**
	 * Job statuses.
	 * @see JobManager::getJobsObjectsOverview()
	 */
	const JS_GROUP_SUCCESSFUL = 'successful';
	const JS_GROUP_UNSUCCESSFUL = 'unsuccessful';
	const JS_GROUP_WARNING = 'warning';
	const JS_GROUP_RUNNING = 'running';
	const JS_GROUP_ALL_TERMINATED = 'all_terminated';

	/**
	 * Job result modes.
	 * Modes:
	 *  - normal - job record list without any additional data
	 *  - overview - job record list with some summary (successful, unsuccessful, warning...)
	 *  - group - job record list with grouped by jobid (jobids as keys)
	 */
	const JOB_RESULT_MODE_NORMAL = 'normal';
	const JOB_RESULT_MODE_OVERVIEW = 'overview';
	const JOB_RESULT_MODE_GROUP = 'group';

	/**
	 * Job result record view.
	 * Views:
	 *  - basic - list only limited record properties
	 *  - full - list all record properties
	 */
	const JOB_RESULT_VIEW_BASIC = 'basic';
	const JOB_RESULT_VIEW_FULL = 'full';
	const JOB_RESULT_VIEW_ADVANCED = 'advanced';

	/**
	 * Get job status groups.
	 *
	 * @return array job status groups
	 */
	private function getJSGroups() {
		return [
			self::JS_GROUP_SUCCESSFUL,
			self::JS_GROUP_UNSUCCESSFUL,
			self::JS_GROUP_WARNING,
			self::JS_GROUP_RUNNING,
			self::JS_GROUP_ALL_TERMINATED
		];
	}

	/**
	 * Get job list.
	 *
	 * @param array $criteria SQL criteria to get job list
	 * @param mixed $limit_val result limit value
	 * @param int $offset_val result offset value
	 * @param string $sort_col sort by selected SQL column (default: JobId)
	 * @param string $sort_order sort order:'ASC' or 'DESC' (default: ASC, ascending)
	 * @param string $mode job result mode (normal, overview, group)
	 * @param string $view job records view (basic, full)
	 * @return array job list records or empty list if no job found
	 */
	public function getJobs($criteria = array(), $limit_val = null, $offset_val = 0, $sort_col = 'JobId', $sort_order = 'ASC', $mode = self::JOB_RESULT_MODE_NORMAL, $view = self::JOB_RESULT_VIEW_FULL) {
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
		    $sort_col = strtolower($sort_col);
		}
		$order = ' ORDER BY ' . $sort_col . ' ' . strtoupper($sort_order);
		$limit = '';
		if (is_int($limit_val) && $limit_val > 0) {
			$limit = ' LIMIT ' . $limit_val;
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}

		$where = Database::getWhere($criteria);

		$job_record = 'Job.*,';
		$join = '';
		if ($view == self::JOB_RESULT_VIEW_BASIC) {
			$job_record = implode(',', $this->basic_mode_job_props) . ',';
		} elseif ($view == self::JOB_RESULT_VIEW_ADVANCED) {
			$job_record = 'Job.*, PriorJob.Name AS priorjobname,';
			$join = ' LEFT JOIN Job AS PriorJob ON (Job.PriorJobId = PriorJob.JobId) ';
		}

		$record = FileSetRecord::finder();
		$tableinfo = $record->getRecordGateway()->getRecordTableInfo($record);
		$content = $tableinfo->getColumns()->itemAt('content') !== null ? 'FileSet.Content' : '\'\'';

		$sql = 'SELECT ' .  $job_record . ' 
Client.Name as client, 
Pool.Name as pool, 
FileSet.FileSet as fileset, 
' . $content . ' AS content 
FROM Job 
JOIN Client USING (ClientId) 
LEFT JOIN Pool USING (PoolId) 
LEFT JOIN FileSet USING (FilesetId)'
. $join . $where['where'] . $order . $limit . $offset;

		$statement = Database::runQuery($sql, $where['params']);
		$result = [];
		if ($mode == self::JOB_RESULT_MODE_OVERVIEW) {
			// Overview mode.
			$result = $statement->fetchAll(\PDO::FETCH_OBJ);
			$result = [
				'jobs' => $result,
				'overview' => $this->getJobCountByJSGroup($criteria)
			];
		} elseif ($mode == self::JOB_RESULT_MODE_GROUP) {
			// Group mode.
			$result = $statement->fetchAll(\PDO::FETCH_GROUP | \PDO::FETCH_OBJ);
		} else {
			// Normal mode.
			$result = $statement->fetchAll(\PDO::FETCH_OBJ);
		}
		return $result;
	}

	/**
	 * Get job records with objects.
	 *
	 * @param array $criteria SQL criteria to get job list
	 * @param mixed $limit_val result limit value
	 * @param int $offset_val result offset value
	 * @param array $sort order by and order direction in form [[by1, direction1], [by2, direction2]...etc.]
	 * @param string $view job records view (basic, full)
	 * @return array job record list with objects or empty list if no job found
	 */
	public function getJobsObjectsOverview($criteria = array(), $limit_val = null, $offset_val = 0, $sort = [['EndTime', 'DESC']],  $view = self::JOB_RESULT_VIEW_FULL) {

		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();

		$result = [];
		try {
			// start transaction
			$pdo->beginTransaction();

			$sort_copy = $sort;
			for ($i = 0, $j = 0; $i < count($sort); $i++, $j++) {
				if (strcasecmp($sort[$i][0], 'jobstatus') == 0) {
					// if one from sorting condition is jobstatus, add sorting by joberrors as next element
					array_splice($sort_copy, ($j+1), 0, [['joberrors', $sort_copy[$j][1]]]);

					// take into account $sort_copy counter
					$j++;
				}
			}
			$sort = $sort_copy;

			$order = Database::getOrder($sort);

			// create temporary table
			$jobid_jobstatus_tname = 'jobid_jobstatus_' . getmypid();
			$db_params = $this->getModule('api_config')->getConfig('db');
			$table_type = '';
			if ($db_params['type'] === Database::PGSQL_TYPE) {
				/**
				 * For PostgreSQL there is used temporary table. For MySQL using temporary
				 * tables is impossible because of problems with support:
				 * @see https://dev.mysql.com/doc/refman/8.0/en/temporary-table-problems.html
				 */
				$table_type = 'TEMPORARY';
			}
			$where = Database::getWhere($criteria);
			$sql = 'CREATE ' . $table_type . ' TABLE ' . $jobid_jobstatus_tname . ' AS
				SELECT DISTINCT Job.JobId,
					Job.JobStatus,
					Job.JobErrors,
					Client.Name AS client,
					Pool.Name AS pool,
					FileSet.FileSet AS fileset,
					FileSet.Content AS content
				FROM Job
				JOIN Client USING (ClientId)
				LEFT JOIN Pool USING (PoolId)
				LEFT JOIN FileSet USING (FilesetId)
				LEFT JOIN Object USING (JobId)
				' . $where['where'];

			Database::execute($sql, $where['params']);

			// get job status count
			$sql = 'SELECT 
				CASE
					WHEN JobErrors > 0 AND JobStatus = \'T\' THEN \'W\'
					WHEN JobErrors = 0 AND JobStatus = \'T\' THEN \'T\'
					WHEN JobStatus != \'T\' THEN JobStatus
				END Status,
				COUNT(1) AS count
				FROM ' . $jobid_jobstatus_tname . ' GROUP BY Status';

			$statement = Database::runQuery($sql);
			$jobstatus_count = $statement->fetchAll(PDO::FETCH_ASSOC);

			// get all objects
			$jlimit = is_int($limit_val) && $limit_val > 0 ? ' LIMIT ' . $limit_val : '';
			$offset = is_int($offset_val) && $offset_val > 0 ? ' OFFSET ' . $offset_val : '';
			$job_record = 'Job.*';
			$obj_record = 'Object.*';
			if ($view == self::JOB_RESULT_VIEW_BASIC) {
				$job_record = implode(',', $this->basic_mode_job_props);
				$obj_record = implode(',', ObjectManager::$basic_mode_obj_props);
			}
			$job_record .= ',' . $jobid_jobstatus_tname . '.client, ' . $jobid_jobstatus_tname . '.pool, ' . $jobid_jobstatus_tname . '.fileset, ' . $jobid_jobstatus_tname . '.content';
			$sql = 'SELECT JobId, ' . $obj_record . ' 
				FROM Object
				JOIN ' . $jobid_jobstatus_tname . ' USING (JobId)
				WHERE JobId IN
				(
					(SELECT JobId FROM ' . $jobid_jobstatus_tname . ' WHERE JobStatus IN (\'' . implode('\',\'', $this->js_unsuccessful) . '\') ORDER BY JobId DESC )
					UNION
					(SELECT JobId FROM ' . $jobid_jobstatus_tname . ' WHERE JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND ' . $jobid_jobstatus_tname . '.JobErrors > 0 ORDER BY JobId DESC)
					UNION
					(SELECT JobId FROM ' . $jobid_jobstatus_tname . ' WHERE JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND ' . $jobid_jobstatus_tname . '.JobErrors = 0 ORDER BY JobId DESC)
				)';
			$statement = Database::runQuery($sql);
			$all_objects = $statement->fetchAll(PDO::FETCH_ASSOC | PDO::FETCH_GROUP);


			// get running jobs
			$sql = 'SELECT ' . $job_record . ' 
				FROM ' . $jobid_jobstatus_tname . '
				JOIN Job USING (JobId) 
				WHERE ' . $jobid_jobstatus_tname . '.JobStatus IN (\'' . implode('\',\'', $this->js_running) . '\') ' . $order . $jlimit . $offset;

			$statement = Database::runQuery($sql);
			$running_jobs = $statement->fetchAll(PDO::FETCH_ASSOC);

			// get failed jobs
			$sql = 'SELECT ' . $job_record . ' 
				FROM ' . $jobid_jobstatus_tname . '
				JOIN Job USING (JobId) 
				WHERE ' . $jobid_jobstatus_tname . '.JobStatus IN (\'' . implode('\',\'', $this->js_unsuccessful) . '\')
				' . $order . $jlimit . $offset;

			$statement = Database::runQuery($sql);
			$unsuccessful_jobs = $statement->fetchAll(PDO::FETCH_ASSOC);

			// get warning jobs
			$sql = 'SELECT ' . $job_record . ' 
				FROM ' . $jobid_jobstatus_tname .'
				JOIN Job USING (JobId) 
				WHERE ' . $jobid_jobstatus_tname . '.JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND ' . $jobid_jobstatus_tname . '.JobErrors > 0
				' . $order . $jlimit . $offset;

			$statement = Database::runQuery($sql);
			$warning_jobs = $statement->fetchAll(PDO::FETCH_ASSOC);

			// get successful jobs
			$sql = 'SELECT ' . $job_record . ' 
				FROM ' . $jobid_jobstatus_tname .'
				JOIN Job USING (JobId) 
				WHERE ' . $jobid_jobstatus_tname . '.JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND ' . $jobid_jobstatus_tname . '.JobErrors = 0
				' . $order . $jlimit . $offset;

			$statement = Database::runQuery($sql);
			$successful_jobs = $statement->fetchAll(PDO::FETCH_ASSOC);

			// get all terminated jobs
			$sql = 'SELECT ' . $job_record . ' 
				FROM ' . $jobid_jobstatus_tname . '
				JOIN Job USING (JobId) 
				WHERE ' . $jobid_jobstatus_tname . '.JobStatus NOT IN (\'' . implode('\',\'', $this->js_running) . '\')
				' . $order . $jlimit . $offset;

			$statement = Database::runQuery($sql);
			$all_jobs = $statement->fetchAll(PDO::FETCH_ASSOC);

			// drop temporary table
			$sql = 'DROP TABLE ' . $jobid_jobstatus_tname;

			Database::execute($sql);

			$result = [
				'overview' => []
			];

			$succesful_count = $unsuccesful_count = $warning_count = $running_count = 0;
			for ($i = 0; $i < count($jobstatus_count); $i++) {
				$jobstatus = $jobstatus_count[$i]['status'];
				$count = $jobstatus_count[$i]['count'];
				if (in_array($jobstatus, $this->js_successful)) {
					$succesful_count += $count;
				} elseif (in_array($jobstatus, $this->js_unsuccessful)) {
					$unsuccesful_count += $count;
				} elseif (in_array($jobstatus, $this->js_warning)) {
					$warning_count += $count;
				} elseif (in_array($jobstatus, $this->js_running)) {
					$running_count += $count;
				}
			}
			$count_all = $succesful_count + $unsuccesful_count + $warning_count;
			$result['overview'] = [
				self::JS_GROUP_SUCCESSFUL => [
					'count' => $succesful_count,
					'jobs' => $successful_jobs
				],
				self::JS_GROUP_UNSUCCESSFUL => [
					'count' => $unsuccesful_count,
					'jobs' => $unsuccessful_jobs
				],
				self::JS_GROUP_WARNING => [
					'count' => $warning_count,
					'jobs' => $warning_jobs
				],
				self::JS_GROUP_RUNNING => [
					'count' => $running_count,
					'jobs' => $running_jobs
				],
				self::JS_GROUP_ALL_TERMINATED => [
					'count' => $count_all,
					'jobs' => $all_jobs
				]
			];
			$result['objects'] = $all_objects;
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

	/**
	 * Get job count by job status group.
	 *
	 * @param array $criteria SQL criteria
	 * @return array job count by job status group
	 */
	public function getJobCountByJSGroup($criteria = []) {
		$where = Database::getWhere($criteria, true);
		$cond = '';
		if (!empty($where['where'])) {
			$cond = $where['where'] . ' AND ';
		}
		$sql = 'SELECT 
(SELECT COUNT(1) FROM Job WHERE ' . $cond . ' Job.JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND Job.JobErrors = 0) AS successful,
(SELECT COUNT(1) FROM Job WHERE ' . $cond . ' Job.JobStatus IN (\'' . implode('\',\'', $this->js_unsuccessful) . '\')) AS unsuccessful,
(SELECT COUNT(1) FROM Job WHERE ' . $cond . ' (Job.JobStatus IN (\'' . implode('\',\'', $this->js_warning) . '\') OR (Job.JobStatus IN (\'' . implode('\',\'', $this->js_successful) . '\') AND Job.JobErrors > 0))) AS warning,
(SELECT COUNT(1) FROM Job WHERE ' . $cond . ' Job.JobStatus IN (\'' . implode('\',\'', $this->js_running) . '\')) AS running,
(SELECT COUNT(1) FROM Job WHERE ' . $cond . ' Job.JobStatus NOT IN (\'' . implode('\',\'', $this->js_running) . '\')) AS all_terminated,
(SELECT COUNT(1) FROM Job ' . (!empty($where['where']) ? ' WHERE ' . $where['where'] : '') . ') AS all
		';

		$statement = Database::runQuery($sql, $where['params']);
		return $statement->fetch(\PDO::FETCH_ASSOC);
	}

	/**
	 * Get job record by job identifier.
	 *
	 * @param integer job identifier
	 * @return JobRecord|false job record or false is no job record found
	 */
	public function getJobById($jobid) {
		$job = $this->getJobs(array(
			'Job.JobId' => [[
				'vals' => [$jobid],
				'operator' => 'AND'
			]]
		), 1);
		if (is_array($job) && count($job) > 0) {
			$job = array_shift($job);
		} else {
			$job = false;
		}
		return $job;
	}

	/**
	 * Find all compojobs required to do full restore.
	 *
	 * @param array $jobs jobid to start searching for jobs
	 * @return array compositional jobs regarding given jobid
	 */
	private function findCompositionalJobs(array $jobs) {
		$jobids = [];
		$wait_on_full = false;
		foreach($jobs as $job) {
			if($job->level == 'F') {
				$jobids[] = $job->jobid;
				break;
			} elseif($job->level == 'D' && $wait_on_full === false) {
				$jobids[] = $job->jobid;
				$wait_on_full = true;
			} elseif($job->level == 'I' && $wait_on_full === false) {
				$jobids[] = $job->jobid;
			}
		}
		return $jobids;
	}

	/**
	 * Get latest recent compositional jobids to do restore.
	 *
	 * @param string $jobname job name
	 * @param integer $clientid client identifier
	 * @param integer $filesetid fileset identifier
	 * @param boolean $inc_copy_job determine if include copy jobs to result
	 * @return array list of jobids required to do restore
	 */
	public function getRecentJobids($jobname, $clientid, $filesetid, $inc_copy_job = false) {
		$types = "('B')";
		if ($inc_copy_job) {
			$types = "('B', 'C')";
		}
		$sql = "name='$jobname' AND clientid='$clientid' AND filesetid='$filesetid' AND type IN $types AND jobstatus IN ('T', 'W') AND level IN ('F', 'I', 'D')";
		$finder = JobRecord::finder();
		$criteria = new TActiveRecordCriteria;
		$order1 = 'RealEndTime';
		$order2 = 'JobId';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
		    $order1 = strtolower($order1);
		    $order2 = strtolower($order2);
		}
		$criteria->OrdersBy[$order1] = 'desc';
		$criteria->OrdersBy[$order2] = 'desc';
		$criteria->Condition = $sql;
		$jobs = $finder->findAll($criteria);

		$jobids = array();
		if(is_array($jobs)) {
			$jobids = $this->findCompositionalJobs($jobs);
		}
		return $jobids;
	}

	/**
	 * Get compositional jobids to do restore starting from given job (full/incremental/differential).
	 *
	 * @param integer $jobid job identifier of last job to do restore
	 * @return array list of jobids required to do restore
	 */
	public function getJobidsToRestore($jobid) {
		$jobids = [];
		$bjob = JobRecord::finder()->findBySql(
			"SELECT * FROM Job WHERE jobid = '$jobid' AND jobstatus IN ('T', 'W') AND type IN ('B', 'C') AND level IN ('F', 'I', 'D')"
		);
		if (is_object($bjob)) {
			if ($bjob->level != 'F') {
				$sql = "clientid=:clientid AND filesetid=:filesetid AND type IN ('B', 'C')" .
					" AND jobstatus IN ('T', 'W') AND level IN ('F', 'I', 'D') " .
					" AND starttime <= :starttime and jobid <= :jobid";
				$finder = JobRecord::finder();
				$criteria = new TActiveRecordCriteria;
				$order1 = 'JobId';
				$db_params = $this->getModule('api_config')->getConfig('db');
				if ($db_params['type'] === Database::PGSQL_TYPE) {
					$order1 = strtolower($order1);
				}
				$criteria->OrdersBy[$order1] = 'desc';
				$criteria->Condition = $sql;
				$criteria->Parameters[':clientid'] = $bjob->clientid;
				$criteria->Parameters[':filesetid'] = $bjob->filesetid;
				$criteria->Parameters[':starttime'] = $bjob->endtime;
				$criteria->Parameters[':jobid'] = $bjob->jobid;
				$jobs = $finder->findAll($criteria);

				if(is_array($jobs)) {
					$jobids = $this->findCompositionalJobs($jobs);
				}
			} else {
				$jobids[] = $bjob->jobid;
			}
		}
		return $jobids;
	}

	public function getJobTotals($criteria = [])
	{
		$jobtotals = [
			'job_count' => 0,
			'most_occupied_client' => 0,
			'most_occupied_client_count' => 0,
			'most_occupied_job' => 0,
			'most_occupied_job_count' => 0,
			'most_occupied_pool' => 0,
			'most_occupied_pool_count' => 0,
			'bytes' => 0,
			'files' => 0
		];
		$where = Database::getWhere($criteria);

		/**
		 * NOTE: All SQL queries could be provided in one query.
		 * It could speed up the loading but there is risk that
		 * the query can be too long (many jobs in where clause).
		 * Safely the queries are provided separately.
		 */

		// Job count and total bytes
		$sql = "SELECT
				COUNT(1)          AS job_count,
				SUM(Job.JobFiles) AS files,
				SUM(Job.JobBytes) AS bytes
			FROM
				Job
			{$where['where']}";

		$statement = Database::runQuery($sql, $where['params']);
		$ret = $statement->fetch(\PDO::FETCH_ASSOC);
		if ($ret) {
			$jobtotals = array_merge($jobtotals, $ret);
		}

		// The most occupied client job stats
		$sql = "SELECT
				Client.Name       AS most_occupied_client,
				COUNT(1)          AS most_occupied_client_count
			FROM
				Job
				JOIN Client USING (ClientId)
			{$where['where']}
			GROUP BY Client.Name
			ORDER BY most_occupied_client_count DESC
			LIMIT 1";
		$statement = Database::runQuery($sql, $where['params']);
		$ret = $statement->fetch(\PDO::FETCH_ASSOC);
		if ($ret) {
			$jobtotals = array_merge($jobtotals, $ret);
		}

		// The most occupied job stats
		$sql = "SELECT
				Name     AS most_occupied_job,
				COUNT(1) AS most_occupied_job_count
			FROM
				Job
			{$where['where']}
			GROUP BY Name
			ORDER BY most_occupied_job_count DESC
			LIMIT 1";
		$statement = Database::runQuery($sql, $where['params']);
		$ret = $statement->fetch(\PDO::FETCH_ASSOC);
		if ($ret) {
			$jobtotals = array_merge($jobtotals, $ret);
		}

		// The most occupied pool job stats
		$sql = "SELECT
				Pool.Name AS most_occupied_pool,
				COUNT(1)  AS most_occupied_pool_count
			FROM
				Job
				JOIN Pool USING (PoolId)
			{$where['where']}
			GROUP BY Pool.Name
			ORDER BY most_occupied_pool_count DESC
			LIMIT 1";
		$statement = Database::runQuery($sql, $where['params']);
		$ret = $statement->fetch(\PDO::FETCH_ASSOC);
		if ($ret) {
			$jobtotals = array_merge($jobtotals, $ret);
		}
		return $jobtotals;
	}

	/**
	 * Get jobs stored on given volume.
	 *
	 * @param string $mediaid volume identifier
	 * @param array $allowed_jobs jobs allowed to show
	 * @return array jobs stored on volume
	 */
	public function getJobsOnVolume($mediaid, $allowed_jobs = array()) {
		$jobs_criteria = '';
		if (count($allowed_jobs) > 0) {
			$jobs_sql = implode("', '", $allowed_jobs);
			$jobs_criteria = " AND Job.Name IN ('" . $jobs_sql . "')";
		}
		$sql = "SELECT DISTINCT Job.*, 
Client.Name as client, 
Pool.Name as pool, 
FileSet.FileSet as fileset 
FROM Job 
LEFT JOIN Client USING (ClientId) 
LEFT JOIN Pool USING (PoolId) 
LEFT JOIN FileSet USING (FilesetId) 
LEFT JOIN JobMedia USING (JobId) 
WHERE JobMedia.MediaId='$mediaid' $jobs_criteria";
		return JobRecord::finder()->findAllBySql($sql);
	}

	/**
	 * Get jobs for given client.
	 *
	 * @param string $clientid client identifier
	 * @param array $allowed_jobs jobs allowed to show
	 * @return array jobs for specific client
	 */
	public function getJobsForClient($clientid, $allowed_jobs = array()) {
		$where = '';
		if (count($allowed_jobs) > 0) {
			$criteria = [
				'Job.Name' => [[
					'vals' => $allowed_jobs,
					'operator' => 'OR'
				]]
			];
			$where = Database::getWhere($criteria, true);
			$wh = '';
			if (count($where['params']) > 0) {
				$wh = ' AND ' . $where['where'];
			}
		}
		$sql = "SELECT DISTINCT Job.*, 
Client.Name as client, 
Pool.Name as pool, 
FileSet.FileSet as fileset 
FROM Job 
LEFT JOIN Client USING (ClientId) 
LEFT JOIN Pool USING (PoolId) 
LEFT JOIN FileSet USING (FilesetId) 
WHERE Client.ClientId='$clientid' $wh";
		return JobRecord::finder()->findAllBySql($sql, $where['params']);
	}

	/**
	 * Get jobs where specific filename is stored
	 *
	 * @param string $clientid client identifier
	 * @param string $filename filename without path
	 * @param boolean $strict_mode if true then it maches exact filename, otherwise with % around filename
	 * @param string $path path to narrow results to one specific path
	 * @param array $allowed_jobs jobs allowed to show
	 * @return array jobs for specific client and filename
	 */
	public function getJobsByFilename($clientid, $filename, $strict_mode = false, $path = '', $allowed_jobs = array()) {
		$jobs_criteria = '';
		if (count($allowed_jobs) > 0) {
			$jobs_sql = implode("', '", $allowed_jobs);
			$jobs_criteria = " AND Job.Name IN ('" . $jobs_sql . "')";
		}

		if ($strict_mode === false) {
			$filename = '%' . $filename . '%';
		}

		$path_criteria = '';
		if (!empty($path)) {
			$path_criteria = ' AND Path.Path = :path ';
		}

		$fname_col = 'Path.Path || File.Filename';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::MYSQL_TYPE) {
			$fname_col = 'CONCAT(Path.Path, File.Filename)';
		}

		$sql = "SELECT Job.JobId AS jobid,
                               Job.Name AS name,
                               $fname_col AS file,
                               Job.StartTime AS starttime,
                               Job.EndTime AS endtime,
                               Job.Type AS type,
                               Job.Level AS level,
                               Job.JobStatus AS jobstatus,
                               Job.JobFiles AS jobfiles,
                               Job.JobBytes AS jobbytes 
                      FROM Client, Job, File, Path 
                      WHERE Client.ClientId='$clientid' 
                            AND Client.ClientId=Job.ClientId 
                            AND Job.JobId=File.JobId 
                            AND File.FileIndex > 0 
                            AND Path.PathId=File.PathId 
                            AND File.Filename LIKE :filename 
		      $jobs_criteria 
		      $path_criteria 
		      ORDER BY starttime DESC";
		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		$sth->bindParam(':filename', $filename, PDO::PARAM_STR, 200);
		if (!empty($path)) {
			$sth->bindParam(':path', $path, PDO::PARAM_STR, 400);
		}
		$sth->execute();
		return $sth->fetchAll(PDO::FETCH_ASSOC);
	}

	/**
	 * Get job file list
	 *
	 * @param integer $jobid job identifier
	 * @param string $type file list type: saved, deleted or all.
	 * @param integer $offset SQL query offset
	 * @param integer $limit SQL query limit
	 * @param string $search search file keyword
	 * @return array jobs job list
	 */
	public function getJobFiles($jobid, $type, $offset = 0, $limit = 100, $search = null, $fetch_group = false) {
		$type_crit = '';
		switch ($type) {
			case 'saved': $type_crit = ' AND FileIndex > 0 '; break;
			case 'deleted': $type_crit = ' AND FileIndex <= 0 '; break;
			case 'all': $type_crit = ''; break;
			default: $type_crit = ' AND FileIndex > 0 '; break;
		}

		$db_params = $this->getModule('api_config')->getConfig('db');
		$search_crit = '';
		if (is_string($search)) {
			$path_col = 'Path.Path';
			$filename_col = 'File.Filename';
			if ($db_params['type'] === Database::MYSQL_TYPE) {
				// Conversion is required because LOWER() and UPPER() do not work with BLOB data type.
				$path_col = "CONVERT($path_col USING utf8mb4)";
				$filename_col = "CONVERT($filename_col USING utf8mb4)";
			}
			$search_crit = " AND (LOWER($path_col) LIKE LOWER('%$search%') OR LOWER($filename_col) LIKE LOWER('%$search%')) ";
		}

		$fname_col = 'Path.Path || File.Filename';
		if ($db_params['type'] === Database::MYSQL_TYPE) {
			$fname_col = 'CONCAT(Path.Path, File.Filename)';
		}

		$limit_sql = '';
		if ($limit) {
			$limit_sql = ' LIMIT ' . $limit;
		}

		$offset_sql = '';
		if ($offset) {
			$offset_sql = ' OFFSET ' . $offset;
		}

		$sql = "SELECT $fname_col  AS file, 
                               F.lstat     AS lstat, 
                               F.fileindex AS fileindex 
                        FROM ( 
                            SELECT PathId     AS pathid, 
                                   Lstat      AS lstat, 
                                   FileIndex  AS fileindex, 
                                   FileId     AS fileid 
                            FROM 
                                File 
                            WHERE 
                                JobId=$jobid 
                                $type_crit 
                            UNION ALL 
                            SELECT PathId         AS pathid, 
                                   File.Lstat     AS lstat, 
                                   File.FileIndex AS fileindex, 
                                   File.FileId    AS fileid 
                                FROM BaseFiles 
                                JOIN File ON (BaseFiles.FileId = File.FileId) 
                                WHERE 
                                   BaseFiles.JobId = $jobid 
                        ) AS F, File, Path 
                        WHERE File.FileId = F.FileId AND Path.PathId = F.PathId 
                        $search_crit 
			$limit_sql $offset_sql";
		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		$sth->execute();
		$result = [];
		if ($fetch_group) {
			$result = $sth->fetchAll(PDO::FETCH_COLUMN);
		} else {
			$result = $sth->fetchAll(PDO::FETCH_ASSOC);

			// decode LStat value
			if (is_array($result)) {
				$blstat = $this->getModule('blstat');
				$result_len = count($result);
				for ($i = 0; $i < $result_len; $i++) {
					$result[$i]['lstat'] = $blstat->lstat_human($result[$i]['lstat']);
				}
			}
		}
		return $result;
	}

	public function getNumberOfJobs($criteria) {
		$where = Database::getWhere($criteria);

		$sql = 'SELECT
			Type      AS type,
			SUM(1)    AS total,
			JobStatus AS jobstatus
		FROM Job
		' . (!empty($where['where']) ? $where['where'] : '') . '
		GROUP BY type, jobstatus';

		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->prepare($sql);
		$sth->execute($where['params']);
		return $sth->fetchAll(PDO::FETCH_ASSOC);
	}

	/**
	 * Get job estimation values based on job history.
	 * For PostgreSQL catalog the byte and file values are computed using linear regression.
	 * For MySQL and SQLite catalog the byte and file values are average values
	 * It returns array in form:
	 *  [
	 *  	'bytes_est' => estimated job bytes
	 *  	'bytes_corr' => correlation of the historical size values
	 *  	'files_est' => estimated job files
	 *  	'files_corr' => correlation of the historical file values
	 *  	'job_count' => number of jobs taken into account
	 *  	'avg_duration' => average job duration per job level,
	 *  	'success_perc' => percentage usage successful jobs (for all job levels)
	 *  ]
	 *
	 * @param string $job job name
	 * @param string $level job level letter
	 * @return array|bool job estimation values
	 */
	public function getJobEstimatation($job, $level) {
		$now = time();
		$q = '';
		$sql = '';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
			$sql = 'SELECT
					COALESCE(CORR(jobbytes, jobtdate), 0) AS corr_jobbytes,
					(' . $now . ' * REGR_SLOPE(jobbytes, jobtdate) + REGR_INTERCEPT(jobbytes, jobtdate)) AS jobbytes,
					COALESCE(CORR(jobfiles, jobtdate), 0) AS corr_jobfiles,
					(' . $now . ' * REGR_SLOPE(jobfiles, jobtdate) + REGR_INTERCEPT(jobfiles, jobtdate)) AS jobfiles,
					COUNT(1) AS nb_jobs';
		} else {
			$sql = 'SELECT
					0.1 AS corr_jobbytes,
					AVG(jobbytes) AS jobbytes,
					0.1 AS corr_jobfiles,
					AVG(jobfiles) AS jobfiles,
					COUNT(1) AS nb_jobs';
		}

		if ($level == 'D') {
			$q = 'AND Job.StartTime > (
				SELECT StartTime
				FROM Job
				WHERE Job.Name = \'' . $job . '\'
				AND Job.Level = \'F\'
				AND Job.JobStatus IN (\'T\', \'W\')
				ORDER BY Job.StartTime DESC LIMIT 1
			)';
		}

		$sql .= '
			FROM (
				SELECT JobBytes AS jobbytes,
					JobFiles AS jobfiles,
					JobTDate AS jobtdate
				FROM Job
				WHERE Job.Name = \'' . $job . '\'
				AND Job.Level = \'' . $level . '\'
				AND Job.JobStatus IN (\'T\', \'W\')
				' . $q . '
				ORDER BY StartTime DESC
				LIMIT 4
			) AS temp';

		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->query($sql);
		$result = $sth->fetch(PDO::FETCH_ASSOC);
		$duration = $this->getJobHistoryDuration($job, $level);
		$success = $this->getJobHistorySuccessPercent($job);
		$objects = $this->getJobHistoryAverageObjects($job, $level);
		$bytes_est = (int) $result['jobbytes'];
		$files_est = (int) $result['jobfiles'];
		$corr_jobbytes = (float) $result['corr_jobbytes'];
		$nb_jobs = (int) $result['nb_jobs'];
		$corr_jobfiles = (float) $result['corr_jobfiles'];
		$avg_duration = (int) $duration['duration'];
		$avg_objects = (int) $objects['objects'];
		$success_perc = (int) $success['success'];
		return [
			'bytes_est' => max($bytes_est, 0),
			'bytes_corr' => $corr_jobbytes,
			'files_est' => max($files_est, 0),
			'files_corr' => $corr_jobfiles,
			'job_count' => $nb_jobs,
			'avg_duration' => $avg_duration,
			'avg_objects' => $avg_objects,
			'success_perc' => $success_perc
		];
	}

	/**
	 * Get average job duration by job.
	 * NOTE: It is job duration per job level, not overall job duration for
	 * all job statuses.
	 *
	 * @param string $job job name
	 * @param string $level backup job level
	 * @return array|bool average job duration or false if no job found
	 */
	public function getJobHistoryDuration($job, $level) {
		$duration = '';
		$db_params = $this->getModule('api_config')->getConfig('db');

		if ($db_params['type'] === Database::PGSQL_TYPE) {
			$duration = 'date_part(\'epoch\', EndTime) -  date_part(\'epoch\', StartTime)';
		} elseif ($db_params['type'] === Database::MYSQL_TYPE) {
			$duration = 'UNIX_TIMESTAMP(EndTime) - UNIX_TIMESTAMP(StartTime)';
		} elseif ($db_params['type'] === Database::SQLITE_TYPE) {
			$duration = 'strftime(\'%s\', EndTime) -  strftime(\'%s\', StartTime)';
		}

		$sql = 'SELECT AVG(' . $duration . ') AS duration
			FROM Job
			WHERE Name=\'' . $job . '\' AND Level=\'' . $level . '\'';

		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->query($sql);
		return $sth->fetch(PDO::FETCH_ASSOC);
	}

	/**
	 * Get percentage value of successful jobs by job.
	 *
	 * @param string $job job name
	 * @return array|bool percentage success ratio or false if no job found
	 */
	public function getJobHistorySuccessPercent($job) {
		$sql = 'SELECT (COUNT(*) * 100.0 / NULLIF((SELECT COUNT(*) FROM Job WHERE Name=\'' . $job . '\'), 0)) as success
			FROM Job
			WHERE Name=\'' . $job . '\' AND JobStatus=\'T\'';
		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->query($sql);
		return $sth->fetch(PDO::FETCH_ASSOC);
	}

	/**
	 * Get average value of objects backed up by job.
	 *
	 * @param string $job job name
	 * @param string $level backup job level
	 * @return array|bool average backed up by job object number or false
	 *                    if no job found
	 */
	public function getJobHistoryAverageObjects($job, $level) {
		$sql = 'SELECT AVG(f.counter) AS objects FROM (
			SELECT  Object.JobId AS jobid,
				COUNT(1) AS counter
			FROM Object
			LEFT JOIN Job USING (JobId)
			WHERE Job.Name=\'' . $job . '\' AND Job.Level=\'' . $level . '\' AND Job.JobStatus=\'T\'
			GROUP BY Object.JobId
		) AS f';
		$connection = JobRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();
		$sth = $pdo->query($sql);
		return $sth->fetch(PDO::FETCH_ASSOC);
	}
}
?>
