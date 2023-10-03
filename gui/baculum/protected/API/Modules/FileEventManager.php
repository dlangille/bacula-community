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
 * File event manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class FileEventManager extends APIModule {

	/**
	 * Get file event list.
	 *
	 * @param array $criteria list of optional query criterias
	 * @param int $limit_val limit results value
	 * @param int $offset_val offset results value
	 */
	public function getFileEvents($criteria = [], $limit_val = 0, $offset_val = 0) {
		$sort_col = 'Id';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
		    $sort_col = strtolower($sort_col);
		}
		$order = ' ORDER BY ' . $sort_col . ' DESC';
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = ' LIMIT ' . $limit_val;
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}

		$where = Database::getWhere($criteria);

		$sql = 'SELECT
				FileEvents.*,
				Path.Path,
				File.Filename
			FROM FileEvents
			LEFT JOIN File ON (FileEvents.JobId = File.JobId AND FileEvents.FileIndex = File.FileIndex)
			LEFT JOIN Path USING (PathId)
			' . $where['where'] . $order . $limit . $offset;

		return FileEventRecord::finder()->findAllBySql($sql, $where['params']);
	}

	/**
	 * Get single file event record by id.
	 *
	 * @param integer $id file event identifier
	 * @return FileEventRecord single file event record or null on failure
	 */
	public function getEventById($id) {
		$params = [
			'FileEvents.Id' => [[
				'vals' => $id,
			]]
		];
		$obj = $this->getFileEvents($params, 1);
		if (is_array($obj) && count($obj) > 0) {
			$obj = array_shift($obj);
		}
		return $obj;
	}
}
