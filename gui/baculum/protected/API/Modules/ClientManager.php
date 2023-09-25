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

namespace Baculum\API\Modules;

use Prado\Data\ActiveRecord\TActiveRecordCriteria;

/**
 * Client manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class ClientManager extends APIModule {

	/**
	 * Result modes.
	 */
	const CLIENT_RESULT_MODE_NORMAL = 'normal';
	const CLIENT_RESULT_MODE_OVERVIEW = 'overview';

	/**
	 * Uname pattern to split values.
	 */
	const CLIENT_UNAME_PATTERN = '/^(?P<version>[\w\d\.]+)\s+\((?P<release_date>[\da-z]+\))?\s+(?P<os>.+)$/i';

	/**
	 * Get client list.
	 *
	 * @param mixed $limit_val result limit value
	 * @param int $offset_val result offset value
	 * @param array $criteria SQL criteria to get job list
	 * @param string $mode result mode
	 * @return array clients or empty list if no client found
	 */
	public function getClients($limit_val = 0, $offset_val = 0, $criteria = [], $mode = self::CLIENT_RESULT_MODE_NORMAL) {
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = ' LIMIT ' . $limit_val;
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}
		$where = Database::getWhere($criteria);

		$sql = 'SELECT *
FROM Client 
' . $where['where'] . $offset . $limit;
		$statement = Database::runQuery($sql, $where['params']);
		$result = $statement->fetchAll(\PDO::FETCH_OBJ);
		$this->setSpecialFields($result);
		if ($mode == self::CLIENT_RESULT_MODE_OVERVIEW) {
			$sql = 'SELECT COUNT(1) AS count FROM Client ' . $where['where'];
			$statement = Database::runQuery($sql, $where['params']);
			$count = $statement->fetch(\PDO::FETCH_OBJ);
			if (!is_object($count)) {
				$count = (object)['count' => 0];
			}
			$result = [
				'clients' => $result,
				'count' => $count->count
			];
		}
		return $result;
	}

	/**
	 * Set special client properties.
	 *
	 * @param array $result client results (note: reference)
	 */
	private function setSpecialFields(&$result) {
		for ($i = 0; $i < count($result); $i++) {
			// Add operating system info and client version
			$result[$i]->os = '';
			$result[$i]->version = '';
			if (preg_match(self::CLIENT_UNAME_PATTERN, $result[$i]->uname, $match) === 1) {
				$result[$i]->os = $match['os'];
				$result[$i]->version = $match['version'];
			}
		}
	}

	public function getClientByName($name) {
		$result = ClientRecord::finder()->findByName($name);
		if (is_object($result)) {
			$result = [$result];
			$this->setSpecialFields($result);
			$result = array_shift($result);
		}
		return $result;
	}

	public function getClientById($id) {
		$result = ClientRecord::finder()->findByclientid($id);
		if (is_object($result)) {
			$result = [$result];
			$this->setSpecialFields($result);
			$result = array_shift($result);
		}
		return $result;
	}
}
?>
