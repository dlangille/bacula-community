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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\API\Modules\SourceManager;
use Baculum\Common\Modules\Errors\SourceError;

/**
 * Sources endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Sources extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$limit = $this->Request->contains('limit') && $misc->isValidInteger($this->Request['limit']) ? (int)$this->Request['limit'] : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$job = $this->Request->contains('job') && $misc->isValidName($this->Request['job']) ? $this->Request['job'] : null;
		$client = $this->Request->contains('client') && $misc->isValidName($this->Request['client']) ? $this->Request['client'] : null;
		$fileset = $this->Request->contains('fileset') && $misc->isValidName($this->Request['fileset']) ? $this->Request['fileset'] : null;
		$starttime_from = $this->Request->contains('starttime_from') && $misc->isValidInteger($this->Request['starttime_from']) ? (int)$this->Request['starttime_from'] : null;
		$starttime_to = $this->Request->contains('starttime_to') && $misc->isValidInteger($this->Request['starttime_to']) ? (int)$this->Request['starttime_to'] : null;
		$endtime_from = $this->Request->contains('endtime_from') && $misc->isValidInteger($this->Request['endtime_from']) ? (int)$this->Request['endtime_from'] : null;
		$endtime_to = $this->Request->contains('endtime_to') && $misc->isValidInteger($this->Request['endtime_to']) ? (int)$this->Request['endtime_to'] : null;
		$jobstatus = $this->Request->contains('jobstatus') && $misc->isValidState($this->Request['jobstatus']) ? $this->Request['jobstatus'] : '';
		$hasobject = $this->Request->contains('hasobject') && $misc->isValidBoolean($this->Request['hasobject']) ? $this->Request['hasobject'] : null;
		$order_by = $this->Request->contains('order_by') && $misc->isValidColumn($this->Request['order_by']) ? $this->Request['order_by']: null;
		$order_direction = $this->Request->contains('order_direction') && $misc->isValidOrderDirection($this->Request['order_direction']) ? $this->Request['order_direction']: 'DESC';
		$mode = ($this->Request->contains('overview') && $misc->isValidBooleanTrue($this->Request['overview'])) ? SourceManager::SOURCE_RESULT_MODE_OVERVIEW : SourceManager::SOURCE_RESULT_MODE_NORMAL;
		$joberrors = null;
		if ($this->Request->contains('joberrors') && $misc->isValidBoolean($this->Request['joberrors'])) {
			$joberrors = $misc->isValidBooleanTrue($this->Request['joberrors']) ? true : false;
		}

		$allowed_sort_fields = ['job', 'client', 'fileset', 'starttime', 'endtime', 'jobid', 'content', 'type', 'jobstatus', 'joberrors'];
		if (is_string($order_by) && !in_array($order_by, $allowed_sort_fields)) {
			$this->output = SourceError::MSG_ERROR_INVALID_PROPERTY;
			$this->error = SourceError::ERROR_INVALID_PROPERTY;
			return;
		}

		$props = [];
		if (!is_null($job)) {
			$props['job'] = $job;
		}
		if (!is_null($client)) {
			$props['client'] = $client;
		}
		if (!is_null($fileset)) {
			$props['fileset'] = $fileset;
		}

		// @TODO: Fix using jres in this place. It can lead to a problem when sres or jres will be changed in manager.
		$params = [];
		if (!empty($jobstatus)) {
			$params['jres.jobstatus'] = [[
				'vals' => $jobstatus
			]];
		}
		if (!is_null($hasobject)) {
			if ($misc->isValidBooleanTrue($hasobject)) {
				$params['Object.ObjectId'][] = [
					'operator' => 'IS NOT',
					'vals' => 'NULL'
				];
			} elseif ($misc->isValidBooleanFalse($hasobject)) {
				$params['Object.ObjectId'][] = [
					'operator' => 'IS',
					'vals' => 'NULL'
				];
			}
		}

		// Start time range
		if (!empty($starttime_from) || !empty($starttime_to)) {
			$params['jres.starttime'] = [];
			if (!empty($starttime_from)) {
				$params['jres.starttime'][] = [
					'operator' => '>=',
					'vals' => date('Y-m-d H:i:s', $starttime_from)
				];
			}
			if (!empty($starttime_to)) {
				$params['jres.starttime'][] = [
					'operator' => '<=',
					'vals' => date('Y-m-d H:i:s', $starttime_to)
				];
			}
		}

		// End time range
		if (!empty($endtime_from) || !empty($endtime_to)) {
			$params['jres.endtime'] = [];
			if (!empty($endtime_from)) {
				$params['jres.endtime'][] = [
					'operator' => '>=',
					'vals' => date('Y-m-d H:i:s', $endtime_from)
				];
			}
			if (!empty($endtime_to)) {
				$params['jres.endtime'][] = [
					'operator' => '<=',
					'vals' => date('Y-m-d H:i:s', $endtime_to)
				];
			}
		}

		if (!is_null($joberrors)) {
			if ($joberrors === true) {
				$params['jres.JobErrors'] = [];
				$params['jres.JobErrors'][] = [
					'operator' => '>',
					'vals' => 0
				];
			} elseif ($joberrors === false) {
				$params['jres.JobErrors'] = [];
				$params['jres.JobErrors'][] = [
					'vals' => 0
				];
			}
		}

		$sources = $this->getModule('source')->getSources(
			$params,
			$props,
			$limit,
			$offset,
			$order_by,
			$order_direction,
			$mode
		);
		$this->output = $sources;
		$this->error = SourceError::ERROR_NO_ERRORS;
	}
}
?>
