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

namespace Baculum\API\Modules;

/**
 * Time manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class TimeManager extends APIModule {

	/**
	 * Director time format.
	 * Example: '29-May-2023 09:01:34'.
	 */
	const DIRECTOR_TIME_FORMAT = 'j-M-Y h:i:s';

	/**
	 * Parse time returned by Director time command.
	 * It is in form: '29-May-2023 09:01:34'
	 * @TODO: Make sure that 'May' is always returned in English
	 *
	 * @param string date/time string from the Director
	 * @return array split date and time single values or empty array if error or warning happens
	 */
	public function parseDirectorTime($time) {
		list($dow, $date_time) = explode(' ', $time, 2);
		$tres = date_parse_from_format(self::DIRECTOR_TIME_FORMAT, $date_time);
		$ret = [];
		if ($tres['warning_count'] === 0 && $tres['error_count'] === 0) {
			$ret = [
				'year' => $tres['year'],
				'month' => $tres['month'],
				'day' => $tres['day'],
				'hour' => $tres['hour'],
				'minute' => $tres['minute'],
				'second' => $tres['second']
			];
		}
		return $ret;
	}
}
