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

use Baculum\API\Modules\ConsoleOutputPage;
use Baculum\API\Modules\ConsoleOutputQueryPage;
use Baculum\Common\Modules\Logging;
use Baculum\Common\Modules\Errors\PluginError;
use Baculum\Common\Modules\Errors\StorageError;

/**
 * Check disk performance on host with Bacula component.
 * So far performance test is possible to do for storage daemon host only.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class PluginCoreDiskPerf extends ConsoleOutputQueryPage {

	public function get() {
		$misc = $this->getModule('misc');
		$storageid = $this->Request->contains('id') && $misc->isValidInteger($this->Request['id']) ? (int)$this->Request['id'] : 0;
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.storage'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			$storage_val = $this->getModule('storage')->getStorageById($storageid);
			if (is_object($storage_val) && in_array($storage_val->name, $result->output)) {
				$storage = $storage_val->name;
			} else {
				$this->output = StorageError::MSG_ERROR_STORAGE_DOES_NOT_EXISTS;
				$this->error = StorageError::ERROR_STORAGE_DOES_NOT_EXISTS;
				return;
			}
		} else {
			$this->output = PluginError::MSG_ERROR_WRONG_EXITCODE;
			$this->error = PluginError::ERROR_WRONG_EXITCODE;
			return;
		}
		$params = [
			'storage' => $storage
		];


		$out_format = ConsoleOutputPage::OUTPUT_FORMAT_RAW;
		if ($this->Request->contains('output') && $this->isOutputFormatValid($this->Request['output'])) {
			$out_format = $this->Request['output'];
		}

		$out = new \StdClass;
		$out->output = [];
		if ($out_format === ConsoleOutputPage::OUTPUT_FORMAT_RAW) {
			$out = $this->getRawOutput($params);
		} elseif($out_format === ConsoleOutputPage::OUTPUT_FORMAT_JSON) {
			$out = $this->getJSONOutput($params);
		}
		$this->output = $out->output;
		if ($out->exitcode != PluginError::ERROR_EXECUTING_PLUGIN_QUERY_COMMAND) {
			if ($out->exitcode != 0) {
				$this->error = PluginError::ERROR_WRONG_EXITCODE;
				$this->output = PluginError::MSG_ERROR_WRONG_EXITCODE;
			} else {
				$this->error = PluginError::ERROR_NO_ERRORS;
			}
		}
	}

	/**
	 * Get diskperf command output in raw format.
	 *
	 * @param array $params command parameters
	 * @return StdClass object with output and exitcode
	 */
	protected function getRawOutput($params = []) {
		$ret = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			[
				'.query',
				'plugin="core:"',
				'storage="' . $params['storage'] . '"',
				'parameter="diskperf"'
			],
			null,
			true
		);
		if ($ret->exitcode != 0) {
			$this->getModule('logging')->log(
				Logging::CATEGORY_EXECUTE,
				'Wrong output from RAW .query diskperf: ' . implode(PHP_EOL, $ret->output)
			);
			$ret->output = []; // don't provide errors to output, only in logs
		} elseif ($this->isError($ret->output)) {
			$ret->exitcode = PluginError::ERROR_EXECUTING_PLUGIN_QUERY_COMMAND;
		}
		return $ret;
	}

	/**
	 * Get query diskperf command output in JSON format.
	 *
	 * @param array $params command parameters
	 * @return StdClass object with output and exitcode
	 */
	protected function getJSONOutput($params = []) {
		$result = $this->getRawOutput($params);
		if ($result->exitcode === 0) {
			$result->output = $this->getItemRows($result->output);
		}
		return $result;
	}

	/**
	 * Parse and get rows with items from output.
	 *
	 * @param array $output dot query command output
	 * @return array parsed output
	 */
	private function getItemRows(array $output) {
		$out = [];
		$pattern = '/^perf (?P<path>.+)? write=(?P<write>[a-zA-Z0-9\/]+) read=(?P<read>[a-zA-Z0-9\/]+) no_cache=(?P<no_cache>[a-z]+)$/i';
		for ($i = 0; $i < count($output); $i++) {
			// perf /tmp write=1707MiB/s read=4067MiB/s no_cache=fadvise
			if (preg_match($pattern, $output[$i], $match) === 1) {
				$out[] = [
					'path' => $match['path'],
					'write' => $match['write'],
					'read' => $match['read'],
					'no_cache' => $match['no_cache']
				];
			}
		}
		return $out;
	}
}
