<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2019 Kern Sibbald
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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\API\Modules\JobRecord;
use Baculum\API\Modules\JobManager;
use Baculum\Common\Modules\Errors\JobError;

/**
 * Jobs endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Jobs extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$jobids = $this->Request->contains('jobids') && $misc->isValidIdsList($this->Request['jobids']) ? $this->Request['jobids'] : '';
		$afterjobid = $this->Request->contains('afterjobid') && $misc->isValidInteger($this->Request['afterjobid']) ? $this->Request['afterjobid'] : 0;
		$limit = $this->Request->contains('limit') && $misc->isValidInteger($this->Request['limit']) ? (int)$this->Request['limit'] : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$jobstatus = $this->Request->contains('jobstatus') ? $this->Request['jobstatus'] : '';
		$level = $this->Request->contains('level') && $misc->isValidJobLevel($this->Request['level']) ? $this->Request['level'] : '';
		$type = $this->Request->contains('type') && $misc->isValidJobType($this->Request['type']) ? $this->Request['type'] : '';
		$jobname = $this->Request->contains('name') && $misc->isValidName($this->Request['name']) ? $this->Request['name'] : '';
		$clientid = $this->Request->contains('clientid') ? $this->Request['clientid'] : '';
		$joberrors = null;
		if ($this->Request->contains('joberrors') && $misc->isValidBoolean($this->Request['joberrors'])) {
			$joberrors = $misc->isValidBooleanTrue($this->Request['joberrors']) ? true : false;
		}

		// UNIX timestamp values
		$schedtime_from = $this->Request->contains('schedtime_from') && $misc->isValidInteger($this->Request['schedtime_from']) ? (int)$this->Request['schedtime_from'] : null;
		$schedtime_to = $this->Request->contains('schedtime_to') && $misc->isValidInteger($this->Request['schedtime_to']) ? (int)$this->Request['schedtime_to'] : null;
		$starttime_from = $this->Request->contains('starttime_from') && $misc->isValidInteger($this->Request['starttime_from']) ? (int)$this->Request['starttime_from'] : null;
		$starttime_to = $this->Request->contains('starttime_to') && $misc->isValidInteger($this->Request['starttime_to']) ? (int)$this->Request['starttime_to'] : null;
		$endtime_from = $this->Request->contains('endtime_from') && $misc->isValidInteger($this->Request['endtime_from']) ? (int)$this->Request['endtime_from'] : null;
		$endtime_to = $this->Request->contains('endtime_to') && $misc->isValidInteger($this->Request['endtime_to']) ? (int)$this->Request['endtime_to'] : null;
		$realstarttime_from = $this->Request->contains('realstarttime_from') && $misc->isValidInteger($this->Request['realstarttime_from']) ? (int)$this->Request['realstarttime_from'] : null;
		$realstarttime_to = $this->Request->contains('realstarttime_to') && $misc->isValidInteger($this->Request['realstarttime_to']) ? (int)$this->Request['realstarttime_to'] : null;
		$realendtime_from = $this->Request->contains('realendtime_from') && $misc->isValidInteger($this->Request['realendtime_from']) ? (int)$this->Request['realendtime_from'] : null;
		$realendtime_to = $this->Request->contains('realendtime_to') && $misc->isValidInteger($this->Request['realendtime_to']) ? (int)$this->Request['realendtime_to'] : null;

		// date/time values
		$schedtime_from_date = $this->Request->contains('schedtime_from_date') && $misc->isValidBDateAndTime($this->Request['schedtime_from_date']) ? $this->Request['schedtime_from_date'] : null;
		$schedtime_to_date = $this->Request->contains('schedtime_to_date') && $misc->isValidBDateAndTime($this->Request['schedtime_to_date']) ? $this->Request['schedtime_to_date'] : null;
		$starttime_from_date = $this->Request->contains('starttime_from_date') && $misc->isValidBDateAndTime($this->Request['starttime_from_date']) ? $this->Request['starttime_from_date'] : null;
		$starttime_to_date = $this->Request->contains('starttime_to_date') && $misc->isValidBDateAndTime($this->Request['starttime_to_date']) ? $this->Request['starttime_to_date'] : null;
		$endtime_from_date = $this->Request->contains('endtime_from_date') && $misc->isValidBDateAndTime($this->Request['endtime_from_date']) ? $this->Request['endtime_from_date'] : null;
		$endtime_to_date = $this->Request->contains('endtime_to_date') && $misc->isValidBDateAndTime($this->Request['endtime_to_date']) ? $this->Request['endtime_to_date'] : null;
		$realstarttime_from_date = $this->Request->contains('realstarttime_from_date') && $misc->isValidBDateAndTime($this->Request['realstarttime_from_date']) ? $this->Request['realstarttime_from_date'] : null;
		$realstarttime_to_date = $this->Request->contains('realstarttime_to_date') && $misc->isValidBDateAndTime($this->Request['realstarttime_to_date']) ? $this->Request['realstarttime_to_date'] : null;
		$realendtime_from_date = $this->Request->contains('realendtime_from_date') && $misc->isValidBDateAndTime($this->Request['realendtime_from_date']) ? $this->Request['realendtime_from_date'] : null;
		$realendtime_to_date = $this->Request->contains('realendtime_to_date') && $misc->isValidBDateAndTime($this->Request['realendtime_to_date']) ? $this->Request['realendtime_to_date'] : null;

		$age = $this->Request->contains('age') && $misc->isValidInteger($this->Request['age']) ? (int)$this->Request['age'] : null;
		$order_by = $this->Request->contains('order_by') && $misc->isValidColumn($this->Request['order_by']) ? $this->Request['order_by']: 'JobId';
		$order_direction = $this->Request->contains('order_direction') && $misc->isValidOrderDirection($this->Request['order_direction']) ? $this->Request['order_direction']: 'DESC';
		$mode = ($this->Request->contains('overview') && $misc->isValidBooleanTrue($this->Request['overview'])) ? JobManager::JOB_RESULT_MODE_OVERVIEW : JobManager::JOB_RESULT_MODE_NORMAL;
		$view = ($this->Request->contains('view') && $misc->isValidResultView($this->Request['view'])) ? $this->Request['view'] : JobManager::JOB_RESULT_VIEW_FULL;
		$priorjobname = $this->Request->contains('priorjobname') && $misc->isValidName($this->Request['priorjobname']) ? $this->Request['priorjobname'] : '';

		if (!empty($jobids)) {
			/**
			 * If jobids parameter provided, all other parameters are not used.
			 */
			$params['Job.JobId'] = [];
			$params['Job.JobId'][] = [
				'operator' => 'OR',
				'vals' => explode(',', $jobids)
			];
			$result = $this->getModule('job')->getJobs(
				$params,
				null,
				0,
				$order_by,
				$order_direction
			);
			$this->output = $result;
			$this->error = JobError::ERROR_NO_ERRORS;
			return;
		}

		if (!empty($clientid) && !$misc->isValidId($clientid)) {
			$this->output = JobError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_CLIENT_DOES_NOT_EXISTS;
			return;
		}

		$client = $this->Request->contains('client') ? $this->Request['client'] : '';
		if (!empty($client) && !$misc->isValidName($client)) {
			$this->output = JobError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_CLIENT_DOES_NOT_EXISTS;
			return;
		}
		$jr = new \ReflectionClass('Baculum\API\Modules\JobRecord');
		$sort_cols = $jr->getProperties();
		$order_by_lc = strtolower($order_by);
		$cols_excl = ['client', 'fileset', 'pool'];
		$columns = [];
		foreach ($sort_cols as $cols) {
			$name = $cols->getName();
			// skip columns not existing in the catalog
			if (in_array($name, $cols_excl)) {
				continue;
			}
			$columns[] = $name;
		}
		if (!in_array($order_by_lc, $columns)) {
			$this->output = JobError::MSG_ERROR_INVALID_PROPERTY;
			$this->error = JobError::ERROR_INVALID_PROPERTY;
			return;
		}


		$params = [];

		if ($afterjobid > 0) {
			$params['Job.JobId'] = [];
			$params['Job.JobId'][] = [
				'operator' => '>',
				'vals' => $afterjobid
			];
		}

		$jobstatuses = array_keys($misc->getJobState());
		$sts = str_split($jobstatus);
		$js_counter = 0;
		for ($i = 0; $i < count($sts); $i++) {
			if (in_array($sts[$i], $jobstatuses)) {
				if (!key_exists('Job.JobStatus', $params)) {
					$params['Job.JobStatus'] = [];
					$params['Job.JobStatus'][$js_counter] = [
						'operator' => 'OR',
						'vals' => []
					];
				}
				$params['Job.JobStatus'][$js_counter]['vals'][] = $sts[$i];
			}
		}
		if (!empty($level)) {
			$params['Job.Level'] = [];
			$params['Job.Level'][] = [
				'vals' => $level
			];
		}
		if (!empty($type)) {
			$params['Job.Type'] = [];
			$params['Job.Type'][] = [
				'vals' => $type
			];
		}

		// Scheduled time range
		if (!empty($schedtime_from) || !empty($schedtime_to) || !empty($schedtime_from_date) || !empty($schedtime_to_date)) {
			$params['Job.SchedTime'] = [];

			// Schedule time from
			$sch_t_from = '';
			if (!empty($schedtime_from)) {
				$sch_t_from = date('Y-m-d H:i:s', $schedtime_from);
			} elseif (!empty($schedtime_from_date)) {
				$sch_t_from = $schedtime_from_date;
			}
			if (!empty($sch_t_from)) {
				$params['Job.SchedTime'][] = [
					'operator' => '>=',
					'vals' => $sch_t_from
				];
			}

			// Schedule time to
			$sch_t_to = '';
			if (!empty($schedtime_to)) {
				$sch_t_to = date('Y-m-d H:i:s', $schedtime_to);
			} elseif (!empty($schedtime_to_date)) {
				$sch_t_to = $schedtime_to_date;
			}
			if (!empty($sch_t_to)) {
				$params['Job.SchedTime'][] = [
					'operator' => '<=',
					'vals' => $sch_t_to
				];
			}
		}

		// Start time range
		if (!empty($starttime_from) || !empty($starttime_to) || !empty($starttime_from_date) || !empty($starttime_to_date)) {
			$params['Job.StartTime'] = [];

			// Start time from
			$sta_t_from = '';
			if (!empty($starttime_from)) {
				$sta_t_from = date('Y-m-d H:i:s', $starttime_from);
			} elseif (!empty($starttime_from_date)) {
				$sta_t_from = $starttime_from_date;
			}
			if (!empty($sta_t_from)) {
				$params['Job.StartTime'][] = [
					'operator' => '>=',
					'vals' => $sta_t_from
				];
			}

			// Start time to
			$sta_t_to = '';
			if (!empty($starttime_to)) {
				$sta_t_to = date('Y-m-d H:i:s', $starttime_to);
			} elseif (!empty($starttime_to_date)) {
				$sta_t_to = $starttime_to_date;
			}
			if (!empty($sta_t_to)) {
				$params['Job.StartTime'][] = [
					'operator' => '<=',
					'vals' => $sta_t_to
				];
			}
		} elseif (!empty($age)) { // Job age (now() - age)
			$params['Job.StartTime'] = [];
			$params['Job.StartTime'][] = [
				'operator' => '>=',
				'vals' => date('Y-m-d H:i:s', (time() - $age))
			];
		}

		// End time range
		if (!empty($endtime_from) || !empty($endtime_to) || !empty($endtime_from_date) || !empty($endtime_to_date)) {
			$params['Job.EndTime'] = [];

			// End time from
			$end_t_from = '';
			if (!empty($endtime_from)) {
				$end_t_from = date('Y-m-d H:i:s', $endtime_from);
			} elseif (!empty($endtime_from_date)) {
				$end_t_from = $endtime_from_date;
			}
			if (!empty($end_t_from)) {
				$params['Job.EndTime'][] = [
					'operator' => '>=',
					'vals' => $end_t_from
				];
			}

			// End time to
			$end_t_to = '';
			if (!empty($endtime_to)) {
				$end_t_to = date('Y-m-d H:i:s', $endtime_to);
			} elseif (!empty($endtime_to_date)) {
				$end_t_to = $endtime_to_date;
			}
			if (!empty($end_t_to)) {
				$params['Job.EndTime'][] = [
					'operator' => '<=',
					'vals' => $end_t_to
				];
			}
		}

		// Real start time range
		if (!empty($realstarttime_from) || !empty($realstarttime_to) || !empty($realstarttime_from_date) || !empty($realstarttime_to_date)) {
			$params['Job.RealStartTime'] = [];

			// Realstart time from
			$realstart_t_from = '';
			if (!empty($realstarttime_from)) {
				$realstart_t_from = date('Y-m-d H:i:s', $realstarttime_from);
			} elseif (!empty($realstarttime_from_date)) {
				$realstart_t_from = $realstarttime_from_date;
			}
			if (!empty($realstart_t_from)) {
				$params['Job.RealStartTime'][] = [
					'operator' => '>=',
					'vals' => $realstart_t_from
				];
			}

			// Realstart time to
			$realstart_t_to = '';
			if (!empty($realstarttime_to)) {
				$realstart_t_to = date('Y-m-d H:i:s', $realstarttime_to);
			} elseif (!empty($realstarttime_to_date)) {
				$realstart_t_to = $realstarttime_to_date;
			}
			if (!empty($realstart_t_to)) {
				$params['Job.RealStartTime'][] = [
					'operator' => '<=',
					'vals' => $realstart_t_to
				];
			}
		}

		// Real end time range
		if (!empty($realendtime_from) || !empty($realendtime_to) || !empty($realendtime_from_date) || !empty($realendtime_to_date)) {
			$params['Job.RealEndTime'] = [];

			// Realend time from
			$realend_t_from = '';
			if (!empty($realendtime_from)) {
				$realend_t_from = date('Y-m-d H:i:s', $realendtime_from);
			} elseif (!empty($realendtime_from_date)) {
				$realend_t_from = $realendtime_from_date;
			}
			if (!empty($realend_t_from)) {
				$params['Job.RealEndTime'][] = [
					'operator' => '>=',
					'vals' => $realend_t_from
				];
			}

			// Realend time to
			$realend_t_to = '';
			if (!empty($realendtime_to)) {
				$realend_t_to = date('Y-m-d H:i:s', $realendtime_to);
			} elseif (!empty($realendtime_to_date)) {
				$realend_t_to = $realendtime_to_date;
			}
			if (!empty($realend_t_to)) {
				$params['Job.RealEndTime'][] = [
					'operator' => '<=',
					'vals' => $realend_t_to
				];
			}
		}

		if ($view == JobManager::JOB_RESULT_VIEW_ADVANCED) {
			$params['PriorJob.Name'] = [];
			$params['PriorJob.Name'][] = [
				'vals' => $priorjobname
			];
		}

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.jobs'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			$vals = [];
			if (!empty($jobname) && in_array($jobname, $result->output)) {
				$vals = [$jobname];
			} else {
				$vals = $result->output;
			}
			if (count($vals) == 0) {
				// no $vals criteria means that user has no job resources assigned.
				$this->output = [];
				$this->error = JobError::ERROR_NO_ERRORS;
				return;
			}

			$params['Job.Name'] = [];
			$params['Job.Name'][] = [
				'operator' => 'OR',
				'vals' => $vals
			];

			$error = false;
			// Client name and clientid filter
			if (!empty($client) || !empty($clientid)) {
				$result = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.client'));
				if ($result->exitcode === 0) {
					array_shift($result->output);
					$cli = null;
					if (!empty($client)) {
						$cli = $this->getModule('client')->getClientByName($client);
					} elseif (!empty($clientid)) {
						$cli = $this->getModule('client')->getClientById($clientid);
					}
					if (is_object($cli) && in_array($cli->name, $result->output)) {
						$params['Job.ClientId'] = [];
						$params['Job.ClientId'][] = [
							'operator' => 'AND',
							'vals' => [$cli->clientid]
						];
					} else {
						$error = true;
						$this->output = JobError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
						$this->error = JobError::ERROR_CLIENT_DOES_NOT_EXISTS;
					}
				} else {
					$error = true;
					$this->output = $result->output;
					$this->error = $result->exitcode;
				}
			}
			if (!is_null($joberrors)) {
				if ($joberrors === true) {
					$params['Job.JobErrors'] = [];
					$params['Job.JobErrors'][] = [
						'operator' => '>',
						'vals' => 0
					];
				} elseif ($joberrors === false) {
					$params['Job.JobErrors'] = [];
					$params['Job.JobErrors'][] = [
						'vals' => 0
					];
				}
			}

			if ($error === false) {
				$result = $this->getModule('job')->getJobs(
					$params,
					$limit,
					$offset,
					$order_by,
					$order_direction,
					$mode,
					$view
				);
				$this->output = $result;
				$this->error = JobError::ERROR_NO_ERRORS;
			}
		} else {
			$this->output = $result->output;
			$this->error = $result->exitcode;
		}
	}
}
?>
