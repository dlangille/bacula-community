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
use Baculum\Common\Modules\Errors\BVFSError;

/**
 * BVFS update.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class BVFSUpdate extends BaculumAPIServer {

	public function set($id, $params) {
		$jobids = null;
		if (property_exists($params, 'jobids') && $this->getModule('misc')->isValidIdsList($params->jobids)) {
			$jobids = $params->jobids;
		}
		
		if (is_null($jobids)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_JOBID_LIST;
			$this->error = BVFSError::ERROR_INVALID_JOBID_LIST;
			return;
		}

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			array('.bvfs_update', 'jobid="' . $jobids . '"')
		);
		$this->output = $result->output;
		$this->error = $result->exitcode;
	}
}
?>
