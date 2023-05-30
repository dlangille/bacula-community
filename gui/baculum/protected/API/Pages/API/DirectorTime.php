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

use Baculum\Common\Modules\Errors\BconsoleError;
use Baculum\Common\Modules\Errors\TimeError;

/**
 * Director time.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class DirectorTime extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$director = $this->Request->contains('name') && $misc->isValidName($this->Request['name']) ? $this->Request['name'] : null;

		$dirs = [];
		$result = $this->getModule('bconsole')->getDirectors();
		if ($result->exitcode === 0) {
			$dirs = $result->output;
		}

		if (is_null($director) || !in_array($director, $dirs)) {
			// Invalid director
			$this->output = BconsoleError::MSG_ERROR_INVALID_DIRECTOR;
			$this->error = BconsoleError::ERROR_INVALID_DIRECTOR;
			return;
		}

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$director,
			['time'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			$result->output = array_filter($result->output);
			if (count($result->output) == 1) {
				$this->output = $this->getModule('time')->parseDirectorTime($result->output[0]);
				if (count($this->output) > 0) {
					$this->error = TimeError::ERROR_NO_ERRORS;
				} else {
					$this->output = TimeError::MSG_ERROR_INVALID_DATE_TIME . ' Output=>' . $result->output[0];
					$this->error = TimeError::ERROR_INVALID_DATE_TIME;
				}
			} else {
				$this->output = TimeError::MSG_ERROR_INVALID_DATE_TIME . ' Output=>' . implode('', $result->output);
				$this->error = TimeError::ERROR_INVALID_DATE_TIME;
			}
		} else {
			$this->output = TimeError::MSG_ERROR_WRONG_EXITCODE . ' Exitcode=' . $result->exitcode;
			$this->error = TimeError::ERROR_WRONG_EXITCODE;
		}
	}
}
