<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2023 Kern Sibbald
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

use Baculum\Common\Modules\Errors\FileEventError;

/**
 * Events endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class FileEvents extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$limit = $this->Request->contains('limit') ? intval($this->Request['limit']) : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$sourcejobid = $this->Request->contains('sourcejobid') && $misc->isValidInteger($this->Request['sourcejobid']) ? (int)$this->Request['sourcejobid'] : null;
		$jobid = $this->Request->contains('jobid') && $misc->isValidInteger($this->Request['jobid']) ? (int)$this->Request['jobid'] : null;
		$fileindex = $this->Request->contains('fileindex') && $misc->isValidInteger($this->Request['fileindex']) ? (int)$this->Request['fileindex'] : null;
		$type = $this->Request->contains('type') && $misc->isValidName($this->Request['type']) ? $this->Request['type'] : null;
		$severity = $this->Request->contains('severity') && $misc->isValidInteger($this->Request['severity']) ? (int)$this->Request['severity'] : null;
		$source = $this->Request->contains('source') && $misc->isValidName($this->Request['source']) ? $this->Request['source'] : null;
		$description = $this->Request->contains('description') && $misc->isValidNameExt($this->Request['description']) ? $this->Request['description'] : null;
		$time_from_date = $this->Request->contains('time_from_date') && $misc->isValidBDateAndTime($this->Request['time_from_date']) ? $this->Request['time_from_date'] : null;
		$time_to_date = $this->Request->contains('time_to_date') && $misc->isValidBDateAndTime($this->Request['time_to_date']) ? $this->Request['time_to_date'] : null;

		$params = [];
		if (!empty($jobid)) {
			$params['FileEvents.JobId'] = [[
				'vals' => $jobid
			]];
		}
		if (!empty($sourcejobid)) {
			$params['FileEvents.SourceJobId'] = [[
				'vals' => $sourcejobid
			]];
		}
		if (!empty($fileindex)) {
			$params['FileEvents.FileIndex'] = [[
				'vals' => $fileindex
			]];
		}
		if (!empty($type)) {
			$params['FileEvents.Type'] = [[
				'vals' => $type
			]];
		}
		if (!empty($severity)) {
			$params['FileEvents.Severity'] = [[
				'vals' => $severity
			]];
		}
		if (!empty($source)) {
			$params['FileEvents.Source'] = [[
				'vals' => $source
			]];
		}
		if (!empty($description)) {
			$params['FileEvents.Description'] = [[
				'vals' => $description
			]];
		}

		if (!empty($time_from_date) || !empty($time_to_date)) {
			$params['FileEvents.Time'] = [];
			if (!empty($time_from_date)) {
				$params['FileEvents.Time'][] = [
					'vals' => $time_from_date,
					'operator' => '>='
				];
			}
			if (!empty($time_to_date)) {
				$params['FileEvents.Time'][] = [
					'vals' => $time_to_date,
					'operator' => '<='
				];
			}
		}

		$fileevents = $this->getModule('fileevent')->getFileEvents(
			$params,
			$limit,
			$offset
		);
		$this->output = $fileevents;
		$this->error = FileEventError::ERROR_NO_ERRORS;
	}
}
