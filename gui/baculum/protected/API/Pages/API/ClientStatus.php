<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2021 Kern Sibbald
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
use Baculum\API\Modules\StatusClient;
use Baculum\Common\Modules\Errors\ClientError;
use Baculum\Common\Modules\Errors\GenericError;

/**
 * Client status.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class ClientStatus extends ConsoleOutputPage {

	public function get() {
		$clientid = $this->Request->contains('id') ? intval($this->Request['id']) : 0;
		$client = $this->getModule('client')->getClientById($clientid);
		$status = $this->getModule('status_fd');
		$type = $this->Request->contains('type') && $status->isValidOutputType($this->Request['type']) ? $this->Request['type'] : null;
		$out_format = $this->Request->contains('output') && $this->isOutputFormatValid($this->Request['output']) ? $this->Request['output'] : parent::OUTPUT_FORMAT_RAW;
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			array('.client'),
			null,
			true
		);

		$client_exists = false;
		if ($result->exitcode === 0) {
			$client_exists = (is_object($client) && in_array($client->name, $result->output));
		}

		if ($client_exists == false) {
			// Client doesn't exist or is not available for user because of ACL restrictions
			$this->output = ClientError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
			$this->error = ClientError::ERROR_CLIENT_DOES_NOT_EXISTS;
			return;
		}
		$out = (object)['output' => [], 'error' => 0];
		if ($out_format === parent::OUTPUT_FORMAT_RAW) {
			$out = $this->getRawOutput(['client' => $client->name]);
		} elseif ($out_format === parent::OUTPUT_FORMAT_JSON) {
			$out = $this->getJSONOutput([
				'client' => $client->name,
				'type' => $type
			]);
			$this->addExtraProperties($type, $out);
		}
		$this->output = $out['output'];
		$this->error = $out['error'];
	}

	protected function getRawOutput($params = []) {
		// traditional status client output
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			[
				'status',
				'client="' . $params['client'] . '"'
			]
		);
		$error = $result->exitcode == 0 ? $result->exitcode : GenericError::ERROR_WRONG_EXITCODE;
		$ret = [
			'output' => $result->output,
			'error' => $error
		];
		return $ret;
	}

	protected function getJSONOutput($params = []) {
		// status client JSON output by API 2 interface
		$status = $this->getModule('status_fd');
		return $status->getStatus(
			$this->director,
			$params['client'],
			$params['type']
		);
	}

	/**
	 * Add extra properties to JSON status client output.
	 *
	 * @param string $type output type (running, terminated...)
	 * @param string $result client output JSON results (be careful - reference)
	 */
	private function addExtraProperties($type, &$result) {
		if ($type == StatusClient::OUTPUT_TYPE_RUNNING) {
			$jobids = [];
			for ($i = 0; $i < count($result['output']); $i++) {
				$jobids[] = $result['output'][$i]['jobid'];
			}
			if (count($jobids) == 0) {
				// no jobs, no extra props
				return;
			}
			$params = [
				'Job.JobId' => [[
					'operator' => 'IN',
					'vals' => $jobids
				]]
			];
			$jobs = $this->getModule('job')->getJobs(
				$params
			);
			for ($i = 0; $i < count($jobids); $i++) {
				for ($j = 0; $j < count($jobs); $j++) {
					if ($jobids[$i] != $jobs[$j]->jobid) {
						continue;
					}
					$result['output'][$i]['name'] = $jobs[$j]->name;
					$result['output'][$i]['fileset'] = $jobs[$j]->fileset;
					break;
				}
			}
		}
	}
}

?>
