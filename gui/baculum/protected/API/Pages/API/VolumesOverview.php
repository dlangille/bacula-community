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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\PoolError;

/**
 * Media/Volumes overview endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class VolumesOverview extends BaculumAPIServer {
	public function get() {
		$misc = $this->getModule('misc');
		$limit = $this->Request->contains('limit') ? intval($this->Request['limit']) : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$pools = $this->getModule('pool')->getPools();
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.pool'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			if (is_array($pools) && count($pools) > 0) {
				$params = [];
				$pools_output = [];
				foreach ($pools as $pool) {
					if (in_array($pool->name, $result->output)) {
						$pools_output[] = $pool->name;
					}
				}
				$params['Pool.Name'] = [];
				$params['Pool.Name'][] = [
					'operator' => 'IN',
					'vals' => $pools_output
				];
				$ret = $this->getModule('volume')->getMediaOverview(
					$params,
					$limit,
					$offset
				);
				$this->output = $ret;
				$this->error = PoolError::ERROR_NO_ERRORS;
			} else {
				$this->output = PoolError::MSG_ERROR_POOL_DOES_NOT_EXISTS;
				$this->error = PoolError::ERROR_POOL_DOES_NOT_EXISTS;
			}
		} else {
			$this->output = PoolError::MSG_ERROR_WRONG_EXITCODE;
			$this->error = PoolError::ERROR_WRONG_EXITCODE . ' Exitcode=> ' . $result->exitcode;
		}
	}
}
?>
