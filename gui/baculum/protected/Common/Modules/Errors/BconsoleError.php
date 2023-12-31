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
 * Bconsole error class.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Errors
 * @package Baculum Common
 */
class BconsoleError extends GenericError {

	const ERROR_BCONSOLE_CONNECTION_PROBLEM = 4;
	const ERROR_INVALID_DIRECTOR = 5;
	const ERROR_BCONSOLE_DISABLED = 11;

	const MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM = 'Problem with connection to bconsole.';
	const MSG_ERROR_INVALID_DIRECTOR = 'Invalid director.';
	const MSG_ERROR_BCONSOLE_DISABLED = 'Bconsole support is disabled.';
}
