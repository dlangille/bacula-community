<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2019 Kern Sibbald
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
use Baculum\Common\Modules\Errors\BVFSError;

/**
 * BVFS restore.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class BVFSRestore extends BaculumAPIServer {

	public function create($params) {
		$misc = $this->getModule('misc');
		$jobids = property_exists($params, 'jobids') ? $params->jobids : null;
		$fileids = property_exists($params, 'fileid') ? $params->fileid : null;
		$dirids = property_exists($params, 'dirid') ? $params->dirid : null;
		/**
		 * Name for findex is wrong. In the next version of API change it
		 * from 'findex' into 'hardlink'.
		 */
		$findexes = property_exists($params, 'findex') ? $params->findex : null;
		$fileindexes = property_exists($params, 'fileindex') ? $params->fileindex : null;
		$objectids = property_exists($params, 'objectid') ? $params->objectid : null;
		$path = property_exists($params, 'path') ? $params->path : null;

		if (!is_null($jobids) && !$misc->isValidIdsList($jobids)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_JOBID_LIST;
			$this->error = BVFSError::ERROR_INVALID_JOBID_LIST;
			return;
		}
		if (!is_null($fileids) && !$misc->isValidIdsList($fileids)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_FILEID_LIST;
			$this->error = BVFSError::ERROR_INVALID_FILEID_LIST;
			return;
		}
		if (!is_null($dirids) && !$misc->isValidIdsList($dirids)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_DIRID_LIST;
			$this->error = BVFSError::ERROR_INVALID_DIRID_LIST;
			return;
		}
		if (!is_null($findexes) && !$misc->isValidIdsList($findexes)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_FILEINDEX_LIST;
			$this->error = BVFSError::ERROR_INVALID_FILEINDEX_LIST;
			return;
		}
		if (!is_null($objectids) && !$misc->isValidIdsList($objectids)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_OBJECTID_LIST;
			$this->error = BVFSError::ERROR_INVALID_OBJECTID_LIST;
			return;
		}

		if (!is_null($path) && !$misc->isValidBvfsPath($path)) {
			$this->output = BVFSError::MSG_ERROR_INVALID_RPATH;
			$this->error = BVFSError::ERROR_INVALID_RPATH;
			return;
		}

		$cmd = array('.bvfs_restore', 'path="' . $path . '"');
		if (is_string($jobids)) {
			array_push($cmd, 'jobid="' . $jobids . '"');
		}
		if (is_string($fileids)) {
			array_push($cmd, 'fileid="' . $fileids . '"');
		}
		if (is_string($dirids)) {
			array_push($cmd, 'dirid="' . $dirids . '"');
		}
		if (is_string($findexes)) {
			array_push($cmd, 'hardlink="' . $findexes . '"');
		}
		if (is_string($fileindexes)) {
			array_push($cmd, 'fileindex="' . $fileindexes . '"');
		}
		if (is_string($objectids)) {
			array_push($cmd, 'objectid="' . $objectids . '"');
		}

		$result = $this->getModule('bconsole')->bconsoleCommand($this->director, $cmd);
		$this->output = $result->output;
		$this->error = $result->exitcode;
	}
}
?>
