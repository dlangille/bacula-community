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
use Baculum\Common\Modules\Errors\VolumeError;

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
		$order_by = $this->Request->contains('order_by') && $misc->isValidColumn($this->Request['order_by']) ? $this->Request['order_by']: 'VolStatus';
		$order_direction = $this->Request->contains('order_direction') && $misc->isValidOrderDirection($this->Request['order_direction']) ? $this->Request['order_direction'] : 'ASC';
		$sec_order_by = $this->Request->contains('sec_order_by') && $misc->isValidColumn($this->Request['sec_order_by']) ? $this->Request['sec_order_by']: 'LastWritten';
		$sec_order_direction = $this->Request->contains('sec_order_direction') && $misc->isValidOrderDirection($this->Request['sec_order_direction']) ? $this->Request['sec_order_direction'] : 'DESC';
		$order = [
			[
				$order_by,
				$order_direction
			],
			[
				$sec_order_by,
				$sec_order_direction
			]
		];
		$jr = new \ReflectionClass('Baculum\API\Modules\VolumeRecord');
		$sort_cols = $jr->getProperties();
		$columns = [];
		foreach ($sort_cols as $cols) {
			$columns[] = $cols->getName();
		}
		$col_err = null;
		for ($i = 0; $i < count($order); $i++) {
			$order_by_lc = strtolower($order[$i][0]);
			if (!in_array($order_by_lc, $columns)) {
				$col_err = $order[$i][0];
				break;
			}
		}
		if ($col_err) {
			$this->output = VolumeError::MSG_ERROR_INVALID_PROPERTY . ' Prop=>' . $col_err;
			$this->error = VolumeError::ERROR_INVALID_PROPERTY;
			return;
		}

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
					$offset,
					$order
				);
				$this->output = $ret;
				$this->error = VolumeError::ERROR_NO_ERRORS;
			} else {
				$this->output = PoolError::MSG_ERROR_POOL_DOES_NOT_EXISTS;
				$this->error = PoolError::ERROR_POOL_DOES_NOT_EXISTS;
			}
		} else {
			$this->output = PoolError::MSG_ERROR_WRONG_EXITCODE . ' Exitcode=> ' . $result->exitcode;
			$this->error = PoolError::ERROR_WRONG_EXITCODE;
		}
	}
}
?>
