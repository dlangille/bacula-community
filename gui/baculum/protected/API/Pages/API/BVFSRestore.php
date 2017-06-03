<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2017 Kern Sibbald
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
 
class BVFSRestore extends BaculumAPIServer {
	public function get() {}

	public function create($params) {
		$jobids = property_exists($params, 'jobids') ? $params->jobids : null;
		$fileids = property_exists($params, 'fileid') ? $params->fileid : null;
		$dirids = property_exists($params, 'dirid') ? $params->dirid : null;
		$path = property_exists($params, 'path') ? $params->path : null;

		$is_valid = true;
		if (!is_null($jobids)) {
			$jobidsList = explode(',', $jobids);
			if(is_array($jobidsList)) {
				for($i = 0; $i < count($jobidsList); $i++) {
					$job = $this->getModule('job')->getJobById($jobidsList[$i]);
					if(is_null($job)) {
						$is_valid = false;
						break;
					}
				}
			} else {
				$is_valid = false;
			}
		} else {
			$is_valid = false;
		}

		if($is_valid === true) {
			if(!is_null($path)) {
				$cmd = array('.bvfs_restore', 'jobid="' .  $jobids . '"', 'path="' . $path . '"');
				if(!is_null($fileids)) {
					array_push($cmd, 'fileid="' . $fileids . '"');
				}
				if(!is_null($dirids)) {
					array_push($cmd, 'dirid="' . $dirids . '"');
				}

				$result = $this->getModule('bconsole')->bconsoleCommand($this->director, $cmd, $this->user);
				$this->output = $result->output;
				$this->error = $result->exitcode;
			} else {
				$this->output = BVFSError::MSG_ERROR_INVALID_RPATH;
				$this->error = BVFSError::ERROR_INVALID_RPATH;
			}
		} else {
			$this->output = BVFSError::MSG_ERROR_JOB_DOES_NOT_EXISTS;
			$this->error = BVFSError::ERROR_JOB_DOES_NOT_EXISTS;
		}
	}
}
?>
