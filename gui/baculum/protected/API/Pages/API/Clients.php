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
use Baculum\API\Modules\ClientManager;
use Baculum\Common\Modules\Errors\ClientError;

/**
 * Clients endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Clients extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$client = $this->Request->contains('name') && $misc->isValidName($this->Request['name']) ? $this->Request['name'] : '';
		$limit = $this->Request->contains('limit') ? intval($this->Request['limit']) : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$plugin = $this->Request->contains('plugin') && $misc->isValidAlphaNumeric($this->Request['plugin']) ? $this->Request['plugin'] : '';
		$mode = $this->Request->contains('overview') && $misc->isValidBooleanTrue($this->Request['overview']) ? ClientManager::CLIENT_RESULT_MODE_OVERVIEW : ClientManager::CLIENT_RESULT_MODE_NORMAL;
		$result = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.client'));
		if ($result->exitcode === 0) {
			$params = [];

			$clients = [];
			if (!empty($client)) {
				if (in_array($client, $result->output)) {
					$clients = [$client];
				} else {
					// no client name provided but not found in the configuration
					$this->output = ClientError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
					$this->error = ClientError::ERROR_CLIENT_DOES_NOT_EXISTS;
					return;
				}
			} else {
				$clients = $result->output;
			}
			if (count($clients) == 0) {
				// no $clients criteria means that user has no client resource assigned.
				$this->output = [];
				$this->error = ClientError::ERROR_NO_ERRORS;
				return;
			}

			$params['Client.Name'] = [];
			$params['Client.Name'][] = [
				'operator' => 'IN',
				'vals' => $clients
			];

			if (!empty($plugin)) {
				$params['Plugins'] = [];
				$params['Plugins'][] = [
					'operator' => 'LIKE',
					'vals' => "%{$plugin}%"
				];
			}
			$clients = $this->getModule('client')->getClients(
				$limit,
				$offset,
				$params,
				$mode
			);
			$this->output = $clients;
			$this->error = ClientError::ERROR_NO_ERRORS;
		} else {

			$this->output = $result->output;
			$this->error = $result->exitcode;
		}
	}
}

?>
