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
use Baculum\Common\Modules\Errors\BconsoleError;

/**
 * Storage resource names endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class StorageResNames extends BaculumAPIServer {
	public function get() {
		$limit = $this->Request->contains('limit') ? (int)$this->Request['limit'] : 0;
		$storages_cmd = ['.storage'];

		$directors = $this->getModule('bconsole')->getDirectors();
		if ($directors->exitcode != 0) {
			$this->output = $directors->output;
			$this->error = $directors->exitcode;
			return;
		}
		$storages = [];
		$error = false;
		$error_obj = null;
		for ($i = 0; $i < count($directors->output); $i++) {
			$storage_list = $this->getModule('bconsole')->bconsoleCommand(
				$directors->output[$i],
				$storages_cmd,
				null,
				true
			);
			if ($storage_list->exitcode != 0) {
				$error_obj = $storage_list;
				$error = true;
				break;
			}
			$storages[$directors->output[$i]] = [];
			for ($j = 0; $j < count($storage_list->output); $j++) {
				if (empty($storage_list->output[$j])) {
					continue;
				}
				$storages[$directors->output[$i]][] = $storage_list->output[$j];

				// limit per director, not for entire elements
				if ($limit > 0 && count($storages[$directors->output[$i]]) === $limit) {
					break;
				}
			}
		}
		if ($error === true) {
			$emsg = ' Output => ' .  implode(PHP_EOL, $error_obj->output) . 'ExitCode => ' . $error_obj->exitcode;
			$this->output = BconsoleError::MSG_ERROR_WRONG_EXITCODE . $emsg;
			$this->error = BconsoleError::ERROR_WRONG_EXITCODE;
		} else {
			$this->output = $storages;
			$this->error =  BconsoleError::ERROR_NO_ERRORS;
		}
	}
}
