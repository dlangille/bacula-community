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
use Baculum\Common\Modules\Errors\BconsoleError;
use Baculum\Common\Modules\Errors\ClientError;
use Baculum\Common\Modules\Errors\PluginVSphereError;

/**
 * List vSphere plugin datastores.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class PluginVSphereListDatastores extends ConsoleOutputQueryPage {

	public function get() {
		$client = null;
		$clientid = $this->Request->contains('id') ? (int)$this->Request['id'] : 0;
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.client'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			$client_val = $this->getModule('client')->getClientById($clientid);
			if (is_object($client_val) && in_array($client_val->name, $result->output)) {
				$client = $client_val->name;
			} else {
				$this->output = ClientError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
				$this->error = ClientError::ERROR_CLIENT_DOES_NOT_EXISTS;
				return;
			}
		} else {
			$this->output = PluginVSphereError::MSG_ERROR_WRONG_EXITCODE;
			$this->error = PluginVSphereError::ERROR_WRONG_EXITCODE;
			return;
		}

		$misc = $this->getModule('misc');
		$out_format = ConsoleOutputPage::OUTPUT_FORMAT_RAW;
		if ($this->Request->contains('output') && $this->isOutputFormatValid($this->Request['output'])) {
			$out_format = $this->Request['output'];
		}

		$plugin = 'vsphere:';
		$server = $this->Request->contains('server') && $misc->isValidName($this->Request['server'])? $this->Request['server'] : '';
		if (!empty($server)) {
			$plugin .= ' server="' . $server . '"';
		}
		$restore_host = $this->Request->contains('restore_host') && $misc->isValidName($this->Request['restore_host'])? $this->Request['restore_host'] : '';
		if (!empty($restore_host)) {
			$plugin .= ' restore_host="' . $restore_host . '"';
		}

		$out = new \StdClass;
		$out->output = [];
		$params = ['client' => $client, 'plugin' => $plugin];
		if ($out_format === ConsoleOutputPage::OUTPUT_FORMAT_RAW) {
			$out = $this->getRawOutput($params);
		} elseif($out_format === ConsoleOutputPage::OUTPUT_FORMAT_JSON) {
			$out = $this->getJSONOutput($params);
		}

		if ($out->exitcode !== 0) {
			$out->error = PluginVSphereError::ERROR_EXECUTING_PLUGIN_QUERY_COMMAND;
			$out->output = PluginVSphereError::MSG_ERROR_EXECUTING_PLUGIN_QUERY_COMMAND . implode(PHP_EOL, $out->output);
			$this->getModule('logging')->log(
				Logging::CATEGORY_EXECUTE,
				$out->output . ", Error={$out->error}"
			);
		} else {
			$out->error = BconsoleError::ERROR_NO_ERRORS;
		}
		$this->output = $out->output;
		$this->error = $out->error;
	}

	/**
	 * Get vSphere datastore list output from console in raw format.
	 *
	 * @param array $params command parameters
	 * @return StdClass object with output and exitcode
	 */
	protected function getRawOutput($params = []) {
		$ret = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			[
				'.query',
				'plugin="' . $params['plugin'] . '"',
				'client="' . $params['client'] . '"',
				'parameter="datastore"'
			]
		);
		if ($ret->exitcode != 0) {
			$this->getModule('logging')->log(
				Logging::CATEGORY_EXECUTE,
				'Wrong output from vSphere RAW datastore list: ' . implode(PHP_EOL, $ret->output)
			);
			$ret->output = []; // don't provide errors to output, only in logs
		} elseif ($this->isError($ret->output)) {
			$ret->exitcode = PluginVSphereError::ERROR_EXECUTING_PLUGIN_QUERY_COMMAND;
		}
		return $ret;
	}

	/**
	 * Get vSphere datastore list output from console in JSON format.
	 *
	 * @param array $params command parameters
	 * @return StdClass object with output and exitcode
	 */
	protected function getJSONOutput($params = []) {
		$result = $this->getRawOutput($params);
		if ($result->exitcode === 0) {
			$rows = $this->getHostRows($result->output);
			$result->output = $this->parseOutputKeyValue($rows);
		}
		return $result;
	}

	/**
	 * Filter rows with datastore items.
	 *
	 * @param array $output dot query command output
	 * @return array
	 */
	private function getHostRows(array $output) {
		$out = [];
		for ($i = 0; $i < count($output); $i++) {
			if (preg_match('/^datastore=/', $output[$i]) === 1) {
				$out[] = $output[$i];
			}
		}
		return $out;
	}
}
