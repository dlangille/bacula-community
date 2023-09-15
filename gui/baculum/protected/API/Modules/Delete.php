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

use Baculum\Common\Modules\Errors\BconsoleError;
use Baculum\Common\Modules\Errors\JobError;

/**
 * Delete command module.
 * The delete command removes records from the catalog.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class Delete extends APIModule {

	/**
	 * Bacula catalog resource types.
	 */
	const TYPE_VOLUME = 'volume';
	const TYPE_POOL = 'pool';
	const TYPE_JOBID = 'jobid';
	const TYPE_SNAPSHOT = 'snapshot'; // not supported yet
	const TYPE_CLIENT = 'client';
	const TYPE_TAG = 'tag';           // not supported yet
	const TYPE_OBJECT = 'object';

	/**
	 * Delete Bacula resources from the catalog.
	 *
	 * @param string $director director name
	 * @param string $type resource type (client, pool, object...etc.)
	 * @param string $value resource identify value to delete
	 * @return array output and error code
	 */
	public function delete($director, $type, $value) {
		$result = [
			'output' => [],
			'error' => BconsoleError::ERROR_NO_ERRORS
		];
		$types = $this->getTypes();
		if (!in_array($type, $types)) {
			$result['output'] = BconsoleError::MSG_ERROR_INVALID_PROPERTY;
			$result['error'] = BconsoleError::ERROR_INVALID_PROPERTY;
		} else {
			$cmd = ['delete'];
			switch ($type) {
				case self::TYPE_JOBID: {
					$jobid = (int)$value;
					$ret = $this->getModule('bconsole')->bconsoleCommand(
						$director,
						['.jobs'],
						null,
						true
					);
					$job = $this->getModule('job')->getJobById($jobid);
					if ($ret->exitcode === 0) {
						if (is_object($job) && in_array($job->name, $ret->output)) {
							$cmd[] = "{$type}=\"{$value}\"";
						} else {
							$result['output'] = JobError::MSG_ERROR_JOB_DOES_NOT_EXISTS;
							$result['error'] = JobError::ERROR_JOB_DOES_NOT_EXISTS;
						}
					} else {
						$result['output'] = $ret->output;
						$result['error'] = $ret->exitcode;
					}
					break;
				}
				case self::TYPE_VOLUME:
				case self::TYPE_POOL:
				case self::TYPE_CLIENT:	{
					$cmd[] = "{$type}=\"{$value}\"";
					break;
				}
				case self::TYPE_OBJECT: {
					$cmd[] = "{$type} objectid=\"{$value}\"";
					break;
				}
				/** Deleting snapshots and tags is not supported by the API so far. **/
				/*case self::TYPE_SNAPSHOT:
				case self::TYPE_TAG: {
					break;
				}*/
			}
			if (count($cmd) > 1 && $result['error'] === 0) {
				$cmd[] = 'yes';
				$ret = $this->getModule('bconsole')->bconsoleCommand(
					$director,
					$cmd
				);
				$result['output'] = $ret->output;
				$result['error'] = $ret->exitcode;
			} else {
				$result['output'] = BconsoleError::MSG_ERROR_INVALID_PROPERTY;
				$result['error'] = BconsoleError::ERROR_INVALID_PROPERTY;
			}
		}
		return $result;
	}

	public function getTypes() {
		return [
			self::TYPE_VOLUME,
			self::TYPE_POOL,
			self::TYPE_JOBID,
			self::TYPE_SNAPSHOT,
			self::TYPE_CLIENT,
			self::TYPE_TAG,
			self::TYPE_OBJECT
		];
	}
}
?>
