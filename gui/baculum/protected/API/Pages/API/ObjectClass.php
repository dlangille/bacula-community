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

use Baculum\API\Modules\Delete;
use Baculum\Common\Modules\Errors\ObjectError;

/**
 * Object endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class ObjectClass extends BaculumAPIServer {

	public function get() {
		$objectid = $this->Request->contains('id') ? (int)$this->Request['id'] : 0;

		$object = $this->getModule('object')->getObjectById($objectid);
		if (is_object($object)) {
			$this->output = $object;
			$this->error = ObjectError::ERROR_NO_ERRORS;
		} else {
			$this->output = ObjectError::MSG_ERROR_OBJECT_DOES_NOT_EXISTS;
			$this->error = ObjectError::ERROR_OBJECT_DOES_NOT_EXISTS;
		}
	}

	public function remove($id) {
		$objectid = (int)$id;
		$object = $this->getModule('object')->getObjectById($objectid);
		if (is_object($object)) {
			$result = $this->getModule('delete')->delete(
				$this->director,
				Delete::TYPE_OBJECT,
				$object->objectid
			);
			$this->output = $result['output'];
			$this->error = $result['error'];
		} else {
			$this->output = ObjectError::MSG_ERROR_OBJECT_DOES_NOT_EXISTS;
			$this->error = ObjectError::ERROR_OBJECT_DOES_NOT_EXISTS;
		}
	}
}
