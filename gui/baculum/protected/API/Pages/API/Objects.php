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

use Baculum\Common\Modules\Errors\ObjectError;
use Baculum\API\Modules\ObjectRecord;
use Baculum\API\Modules\ObjectManager;

/**
 * Objects endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Objects extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$limit = $this->Request->contains('limit') && $misc->isValidInteger($this->Request['limit']) ? (int)$this->Request['limit'] : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$objecttype = $this->Request->contains('objecttype') && $misc->isValidName($this->Request['objecttype']) ? $this->Request['objecttype'] : null;
		$objectname = $this->Request->contains('objectname') && $misc->isValidNameExt($this->Request['objectname']) ? $this->Request['objectname'] : null;
		$objectcategory = $this->Request->contains('objectcategory') && $misc->isValidName($this->Request['objectcategory']) ? $this->Request['objectcategory'] : null;
		$objectsource = $this->Request->contains('objectsource') && $misc->isValidName($this->Request['objectsource']) ? $this->Request['objectsource'] : null;
		$objectuuid = $this->Request->contains('objectuuid') && $misc->isValidName($this->Request['objectuuid']) ? $this->Request['objectuuid'] : null;
		$objectstatus = $this->Request->contains('objectstatus') && $misc->isValidState($this->Request['objectstatus']) ? $this->Request['objectstatus'] : null;
		$jobname = $this->Request->contains('jobname') && $misc->isValidName($this->Request['jobname']) ? $this->Request['jobname'] : null;
		$jobids = $this->Request->contains('jobids') && $misc->isValidIdsList($this->Request['jobids']) ? explode(',', $this->Request['jobids']) : [];
		$jobstatus = $this->Request->contains('jobstatus') && $misc->isValidState($this->Request['jobstatus']) ? $this->Request['jobstatus'] : null;
		$client = $this->Request->contains('client') && $misc->isValidName($this->Request['client']) ? $this->Request['client'] : '';
		$joberrors = null;
		if ($this->Request->contains('joberrors') && $misc->isValidBoolean($this->Request['joberrors'])) {
			$joberrors = $misc->isValidBooleanTrue($this->Request['joberrors']) ? true : false;
		}
		$group_by = $this->Request->contains('groupby') && $misc->isValidColumn($this->Request['groupby']) ? strtolower($this->Request['groupby']) : null;
		$group_offset = $this->Request->contains('group_offset') ? intval($this->Request['group_offset']) : 0;
		$group_limit = $this->Request->contains('group_limit') ? intval($this->Request['group_limit']) : 0;
		$group_order_by = $this->Request->contains('group_order_by') && $misc->isValidColumn($this->Request['group_order_by']) ? $this->Request['group_order_by']: 'ObjectId';
		$group_order_direction = $this->Request->contains('group_order_direction') && $misc->isValidOrderDirection($this->Request['group_order_direction']) ? $this->Request['group_order_direction']: 'ASC';

		// UNIX timestamp values
		$schedtime_from = $this->Request->contains('schedtime_from') && $misc->isValidInteger($this->Request['schedtime_from']) ? (int)$this->Request['schedtime_from'] : null;
		$schedtime_to = $this->Request->contains('schedtime_to') && $misc->isValidInteger($this->Request['schedtime_to']) ? (int)$this->Request['schedtime_to'] : null;
		$starttime_from = $this->Request->contains('starttime_from') && $misc->isValidInteger($this->Request['starttime_from']) ? (int)$this->Request['starttime_from'] : null;
		$starttime_to = $this->Request->contains('starttime_to') && $misc->isValidInteger($this->Request['starttime_to']) ? (int)$this->Request['starttime_to'] : null;
		$endtime_from = $this->Request->contains('endtime_from') && $misc->isValidInteger($this->Request['endtime_from']) ? (int)$this->Request['endtime_from'] : null;
		$endtime_to = $this->Request->contains('endtime_to') && $misc->isValidInteger($this->Request['endtime_to']) ? (int)$this->Request['endtime_to'] : null;
		$realendtime_from = $this->Request->contains('realendtime_from') && $misc->isValidInteger($this->Request['realendtime_from']) ? (int)$this->Request['realendtime_from'] : null;
		$realendtime_to = $this->Request->contains('realendtime_to') && $misc->isValidInteger($this->Request['realendtime_to']) ? (int)$this->Request['realendtime_to'] : null;
		$realstarttime_from = $this->Request->contains('realstarttime_from') && $misc->isValidInteger($this->Request['realstarttime_from']) ? (int)$this->Request['realstarttime_from'] : null;
		$realstarttime_to = $this->Request->contains('realstarttime_to') && $misc->isValidInteger($this->Request['realstarttime_to']) ? (int)$this->Request['realstarttime_to'] : null;

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
		$order_by = $this->Request->contains('order_by') && $misc->isValidColumn($this->Request['order_by']) ? $this->Request['order_by']: 'ObjectId';
		$order_direction = $this->Request->contains('order_direction') && $misc->isValidOrderDirection($this->Request['order_direction']) ? $this->Request['order_direction']: 'DESC';
		$mode = ($this->Request->contains('overview') && $misc->isValidBooleanTrue($this->Request['overview'])) ? ObjectManager::OBJECT_RESULT_MODE_OVERVIEW : ObjectManager::OBJECT_RESULT_MODE_NORMAL;
		$unique_objects = ($this->Request->contains('unique_objects') && $misc->isValidBooleanTrue($this->Request['unique_objects'])) ? true : false;
		$opts = [
			'unique_objects' => $unique_objects
		];

		$or = new \ReflectionClass('Baculum\API\Modules\ObjectRecord');
		$prop_cols = $or->getProperties();
		$cols_excl = ['jobname'];
		$columns = [];
		if (is_string($group_by)) {
			$columns = [];
			foreach ($prop_cols as $cols) {
				$name = $cols->getName();
				// skip columns not existing in the catalog
				if (in_array($name, $cols_excl)) {
					continue;
				}
				$columns[] = $name;
			}

			if (!in_array($group_by, $columns)) {
				$this->output = ObjectError::MSG_ERROR_INVALID_PROPERTY;
				$this->error = ObjectError::ERROR_INVALID_PROPERTY;
				return;
			}
		}

		$order_by_lc = strtolower($order_by);
		$columns = [];
		foreach ($prop_cols as $cols) {
			$name = $cols->getName();
			// skip columns not existing in the catalog
			if (in_array($name, $cols_excl)) {
				continue;
			}
			$columns[] = $name;
		}
		if (!in_array($order_by_lc, $columns)) {
			$this->output = ObjectError::MSG_ERROR_INVALID_PROPERTY;
			$this->error = ObjectError::ERROR_INVALID_PROPERTY;
			return;
		}


		$params = [];
		if (!empty($objecttype)) {
			$params['Object.ObjectType'] = [];
			$params['Object.ObjectType'][] = [
				'vals' => $objecttype
			];
		}
		if (!empty($objectname)) {
			$params['Object.ObjectName'] = [];
			$params['Object.ObjectName'][] = [
				'vals' => $objectname
			];
		}
		if (!empty($objectcategory)) {
			$params['Object.ObjectCategory'] = [];
			$params['Object.ObjectCategory'][] = [
				'vals' => $objectcategory
			];
		}
		if (!empty($objectsource)) {
			$params['Object.ObjectSource'] = [];
			$params['Object.ObjectSource'][] = [
				'vals' => $objectsource
			];
		}
		if (!empty($objectuuid)) {
			$params['Object.ObjectUUID'] = [];
			$params['Object.ObjectUUID'][] = [
				'vals' => $objectuuid
			];
		}
		if (!empty($objectstatus)) {
			$params['Object.ObjectStatus'] = [];
			$params['Object.ObjectStatus'][] = [
				'vals' => $objectstatus
			];
		}
		if (!empty($jobname)) {
			$params['Job.Name'] = [];
			$params['Job.Name'][] = [
				'vals' => $jobname
			];
		}
		if (count($jobids) > 0) {
			$params['Job.JobId'] = [];
			$params['Job.JobId'][] = [
				'operator' => 'IN',
				'vals' => $jobids
			];
		}
		if (!empty($client)) {
			$params['Client.Name'] = [];
			$params['Client.Name'][] = [
				'vals' => $client
			];
		}
		if (!empty($jobstatus)) {
			$params['Job.JobStatus'] = [];
			$params['Job.JobStatus'][] = [
				'vals' => $jobstatus
			];
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

		$objects = $this->getModule('object')->getObjects(
			$params,
			$opts,
			$limit,
			$offset,
			$order_by_lc,
			$order_direction,
			$group_by,
			$group_limit,
			$group_offset,
			$group_order_by,
			$group_order_direction,
			ObjectManager::OBJ_RESULT_VIEW_FULL,
			$mode
		);
		$this->output = $objects;
		$this->error = ObjectError::ERROR_NO_ERRORS;
	}
}
