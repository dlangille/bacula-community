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
 * Device error class.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Errors
 * @package Baculum Common
 */
class DeviceError extends GenericError {

	const ERROR_DEVICE_DEVICE_CONFIG_DOES_NOT_EXIST = 130;
	const ERROR_DEVICE_INVALID_COMMAND = 131;
	const ERROR_DEVICE_AUTOCHANGER_DOES_NOT_EXIST = 132;
	const ERROR_DEVICE_AUTOCHANGER_DRIVE_DOES_NOT_EXIST = 133;
	const ERROR_DEVICE_WRONG_SLOT_NUMBER = 134;
	const ERROR_DEVICE_DRIVE_DOES_NOT_BELONG_TO_AUTOCHANGER = 135;

	const MSG_ERROR_DEVICE_DEVICE_CONFIG_DOES_NOT_EXIST = 'Device config does not exist.';
	const MSG_ERROR_DEVICE_INVALID_COMMAND = 'Invalid changer command.';
	const MSG_ERROR_DEVICE_AUTOCHANGER_DOES_NOT_EXIST = 'Autochanger does not exist.';
	const MSG_ERROR_DEVICE_AUTOCHANGER_DRIVE_DOES_NOT_EXIST = 'Autochanger drive does not exist.';
	const MSG_ERROR_DEVICE_WRONG_SLOT_NUMBER = 'Wrong slot number.';
	const MSG_ERROR_DEVICE_DRIVE_DOES_NOT_BELONG_TO_AUTOCHANGER = 'Drive does not belong to selected autochanger.';
}
