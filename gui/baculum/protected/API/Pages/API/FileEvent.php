
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
 * Event endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class FileEvent extends BaculumAPIServer {

	public function get() {
		$eventid = $this->Request->contains('id') ? (int)$this->Request['id'] : 0;

		$event = $this->getModule('fileevent')->getEventById($eventid);
		if (is_object($event)) {
			$this->output = $event;
			$this->error = FileEventError::ERROR_NO_ERRORS;
		} else {
			$this->output = FileEventError::MSG_ERROR_FILE_EVENT_DOES_NOT_EXIST;
			$this->error = FileEventError::ERROR_FILE_EVENT_DOES_NOT_EXIST;
		}
	}
}
