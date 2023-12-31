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

namespace Baculum\Common\Modules\Errors;

/**
 * Volume error class.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Errors
 * @package Baculum Common
 */
class VolumeError extends GenericError {
	const ERROR_VOLUME_DOES_NOT_EXISTS = 30;
	const ERROR_INVALID_VOLUME = 31;
	const ERROR_INVALID_SLOT = 32;
	const ERROR_VOLUME_ALREADY_EXISTS = 33;

	const MSG_ERROR_VOLUME_DOES_NOT_EXISTS = 'Volume does not exist.';
	const MSG_ERROR_INVALID_VOLUME = 'Invalid volume.';
	const MSG_ERROR_INVALID_SLOT = 'Invalid slot.';
	const MSG_ERROR_VOLUME_ALREADY_EXISTS = 'Volume already exists.';
}
