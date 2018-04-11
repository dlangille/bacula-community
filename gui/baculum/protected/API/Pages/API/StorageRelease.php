<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2018 Kern Sibbald
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
 
class StorageRelease extends BaculumAPIServer {
	public function get() {
		$storageid = $this->Request->contains('id') ? intval($this->Request['id']) : 0;
		$drive = $this->Request->contains('drive') ? intval($this->Request['drive']) : 0;
		$device = $this->Request->contains('device') ? $this->Request['device'] : null;
		$storage = $this->getModule('storage')->getStorageById($storageid);
		if (is_object($storage)) {
			$result = $this->getModule('bconsole')->bconsoleCommand(
				$this->director,
				array(
					'release',
					'storage="' . $storage->name . '"',
					(is_string($device) ? 'device="' . $device . '" drive=0 slot=0' : 'drive=' . $drive . ' slot=0')
				)
			);
			$this->output = $result->output;
			$this->error = $result->exitcode;
		} else {
			$this->output = StorageError::MSG_ERROR_STORAGE_DOES_NOT_EXISTS;
			$this->error = StorageError::ERROR_STORAGE_DOES_NOT_EXISTS;
		}
	}
}

?>
