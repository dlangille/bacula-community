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

use Baculum\API\Modules\ConsoleOutputPage;
use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\JobError;
use Baculum\Common\Modules\Logging;
use Prado\Prado;

/**
 * Run restore command endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class RestoreRun extends ConsoleOutputPage {

	public function create($params) {
		$misc = $this->getModule('misc');
		$jobid = property_exists($params, 'id') && $misc->isValidInteger($params->id) ? intval($params->id) : null;
		$out_format = parent::OUTPUT_FORMAT_RAW; // default output format
		if ($this->Request->contains('output') && $this->isOutputFormatValid($this->Request['output'])) {
			$out_format = $this->Request['output'];
		}
		$client = null;
		if (property_exists($params, 'clientid')) {
			$clientid = intval($params->clientid);
			$client_row = $this->getModule('client')->getClientById($clientid);
			$client = is_object($client_row) ? $client_row->name : null;
		} elseif (property_exists($params, 'client') && $misc->isValidName($params->client)) {
			$client = $params->client;
		}

		$fileset = null;
		if (property_exists($params, 'filesetid')) {
			$filesetid = intval($params->filesetid);
			$fileset_row = $this->getModule('fileset')->getFileSetById($filesetid);
			$fileset = is_object($fileset_row) ? $fileset_row->fileset : null;
		} elseif (property_exists($params, 'fileset') && $misc->isValidName($params->fileset)) {
			$fileset = $params->fileset;
		}

		$rfile = property_exists($params, 'rpath') ? $params->rpath : null;
		$full = property_exists($params, 'full') && $misc->isValidInteger($params->full) ? (bool)$params->full : null;
		$where = property_exists($params, 'where') ? $params->where : null;
		$replace = property_exists($params, 'replace') ? $params->replace : null;

		$restorejob = null;
		if (property_exists($params, 'restorejob') && $misc->isValidName($params->restorejob)) {
			$restorejob = $params->restorejob;
		}
		$strip_prefix = null;
		if (property_exists($params, 'strip_prefix') && $misc->isValidPath($params->strip_prefix)) {
			$strip_prefix = $params->strip_prefix;
		}
		$add_prefix = null;
		if (property_exists($params, 'add_prefix') && $misc->isValidPath($params->add_prefix)) {
			$add_prefix = $params->add_prefix;
		}
		$add_suffix = null;
		if (property_exists($params, 'add_suffix') && $misc->isValidPath($params->add_suffix)) {
			$add_suffix = $params->add_suffix;
		}
		$regex_where = null;
		if (property_exists($params, 'regex_where') && $misc->isValidPath($params->regex_where)) {
			$regex_where = $params->regex_where;
		}

		// Plugin options
		$plugin_options = $plugin_files = $pre_cmds = [];
		if (property_exists($params, 'plugin_options') && is_object($params->plugin_options)) {
			foreach ($params->plugin_options as $objectid => $options) {
				$dir = Prado::getPathOfNamespace('Baculum.API.Config');
				$tmpname = tempnam($dir, 'restore');
				$opts = '';
				foreach ($options as $key => $value) {
					$opts .= "{$key}={$value}" . PHP_EOL;
				}
				if (file_put_contents($tmpname, $opts) !== false) {
					$fileref = "obj{$objectid}";
					$pre_cmds[] = '@putfile';
					$pre_cmds[] = $fileref;
					$pre_cmds[] = $tmpname . PHP_EOL;
					$plugin_options[] = sprintf(
						'pluginrestoreconf="%s:%s"',
						$objectid,
						$fileref
					);
					$plugin_files[] = $tmpname;
				} else {
					$this->Application->getModule('logging')->log(
						Logging::CATEGORY_APPLICATION,
						"Error while writing temporary plugin file $tmpname."
					);
				}
			}
		}

		if(is_null($client)) {
			$this->output = JobError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_CLIENT_DOES_NOT_EXISTS;
			return;
		}

		if(!is_null($rfile) && preg_match($misc::RPATH_PATTERN, $rfile) !== 1) {
			$this->output = JobError::MSG_ERROR_INVALID_RPATH;
			$this->error = JobError::ERROR_INVALID_RPATH;
			return;
		}
		if(!is_null($where) && !$misc->isValidPath($where)) {
			$this->output = JobError::MSG_ERROR_INVALID_WHERE_OPTION;
			$this->error = JobError::ERROR_INVALID_WHERE_OPTION;
			return;
		}

		if(!is_null($replace) && !$misc->isValidReplace($replace)) {
			$this->output = JobError::MSG_ERROR_INVALID_REPLACE_OPTION;
			$this->error = JobError::ERROR_INVALID_REPLACE_OPTION;
			return;
		}


		$command = array('restore',
			'client="' . $client . '"'
		);
		if (count($pre_cmds) > 0) {
			$command = array_merge($pre_cmds, $command);
		}
		if (count($plugin_options) > 0) {
			$command[] = implode(' ', $plugin_options);
		}
		if (is_string($rfile)) {
			// Restore using Bvfs
			$command[] = 'file="?' . $rfile . '"';
		} elseif ($full === true && is_int($jobid) && $jobid > 0 && is_string($fileset)) {
			// Full restore all files
			$command[] = 'jobid="' . $jobid . '"';
			$command[] = 'fileset="' . $fileset . '"';
			$command[] = 'select';
			$command[] = 'all';
			$command[] = 'done';
		}

		if (is_string($replace)) {
			$command[] = 'replace="' . $replace . '"';
		}
		if (is_string($restorejob)) {
			$command[] = 'restorejob="' . $restorejob . '"';
		}
		if (is_string($strip_prefix)) {
			$command[] = 'strip_prefix="' . $strip_prefix . '"';
		}
		if (is_string($add_prefix)) {
			$command[] = 'add_prefix="' . $add_prefix . '"';
		} elseif (is_string($where)) {
			$command[] = 'where="' . $where . '"';
		}
		if (is_string($add_suffix)) {
			$command[] = 'add_suffix="' . $add_suffix . '"';
		}
		if (is_string($regex_where)) {
			$command[] = 'regexwhere="' . $regex_where . '"';
		}
		$command[] = 'yes';

		$restore = $this->getModule('bconsole')->bconsoleCommand($this->director, $command);

		if ($restore->exitcode == 0) {
			// exit code OK, check output
			$queued_jobid = $this->getModule('misc')->findJobIdStartedJob($restore->output);
			if (is_null($queued_jobid)) {
				// new jobid is not detected, error
				$this->error = JobError::ERROR_INVALID_PROPERTY;
				$this->output = JobError::MSG_ERROR_INVALID_PROPERTY . implode(PHP_EOL, $restore->output);
			} else {
				// new jobid is detected, check output format
				if ($out_format == parent::OUTPUT_FORMAT_JSON) {
					// JSON format, return output and new jobid
					$this->output = $this->getJSONOutput([
						'output' => $restore->output,
						'queued_jobid' => $queued_jobid
					]);
				} elseif ($out_format == parent::OUTPUT_FORMAT_RAW) {
					// RAW format, return output
					$this->output = $this->getRawOutput([
						'output' => $restore->output
					]);
				}
				$this->error = JobError::ERROR_NO_ERRORS;
			}
		} else {
			// exit code WRONG
			$this->output = implode(PHP_EOL, $restore->output) . ' Exitcode => ' . $restore->exitcode;
			$this->error = JobError::ERROR_WRONG_EXITCODE;
		}

		// remove temporary plugin files
		for ($i = 0; $i < count($plugin_files); $i++) {
			if (!unlink($plugin_files[$i])) {
				$this->Application->getModule('logging')->log(
					Logging::CATEGORY_APPLICATION,
					"Error while removing temporary plugin file {$plugin_files[$i]}."
				);
			}
		}
	}

	/**
	 * Get raw output from restore run command.
	 * This method will not be called without param.
	 *
	 * @param array output parameter
	 * @return array restore command output
	 */
	protected function getRawOutput($params = []) {
		// Not too much to do here. Required by abstract method.
		return $params['output'];
	}

	/**
	 * Get parsed JSON output from run restore run command.
	 * This method will not be called without params.
	 *
	 * @param array output parameter and queued jobid
	 * @return array output and jobid queued job
	 */
	protected function getJSONOutput($params = []) {
		$output = implode(PHP_EOL, $params['output']);
		$jobid = (int) $params['queued_jobid'];
		return [
			'output' => $output,
			'jobid' => $jobid
		];
	}
}
?>
