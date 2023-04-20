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

namespace Baculum\API\Modules;

use PDO;

/**
 * Volume manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class VolumeManager extends APIModule {

	/**
	 * Volume types (voltype property)
	 */
	const VOLTYPE_FILE = 1;
	const VOLTYPE_TAPE = 2;
	const VOLTYPE_DVD = 3;
	const VOLTYPE_FIFO = 4;
	const VOLTYPE_VTAPE_DEV = 5;
	const VOLTYPE_FTP_DEV = 6;
	const VOLTYPE_VTL_DEV = 7;
	const VOLTYPE_ADATA = 8;
	const VOLTYPE_ALIGNED_DEV = 9;
	const VOLTYPE_DEDUP_OLD_DEV = 10;
	const VOLTYPE_NULL_DEV = 11;
	const VOLTYPE_VALIGNED_DEV = 12;
	const VOLTYPE_VDEDUP_DEV = 13;
	const VOLTYPE_CLOUD_DEV = 14;
	const VOLTYPE_DEDUP_DEV = 15;

	/**
	 * Get disk volume type identifiers.
	 *
	 * @return array disk volume type identifiers
	 */
	private function getDiskVolTypes() {
		return [
			self::VOLTYPE_FILE,
			self::VOLTYPE_ADATA,
			self::VOLTYPE_FIFO,
			self::VOLTYPE_ALIGNED_DEV,
			self::VOLTYPE_DEDUP_OLD_DEV,
			self::VOLTYPE_VALIGNED_DEV,
			self::VOLTYPE_VDEDUP_DEV,
			self::VOLTYPE_CLOUD_DEV,
			self::VOLTYPE_DEDUP_DEV
		];
	}


	/**
	 * Get tape volume type identifiers.
	 *
	 * @return array tape volume type identifiers
	 */
	private function getTapeVolTypes() {
		return [
			self::VOLTYPE_TAPE,
			self::VOLTYPE_VTL_DEV
		];
	}


	/**
	 * Get cloud volume type identifiers.
	 *
	 * @return array cloud volume type identifiers
	 */
	private function getCloudVolTypes() {
		return [
			self::VOLTYPE_CLOUD_DEV
		];
	}

	public function getVolumes($criteria = array(), $limit_val = 0, $offset_val = 0) {
		$order_pool_id = 'PoolId';
		$order_volume = 'VolumeName';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if($db_params['type'] === Database::PGSQL_TYPE) {
		    $order_pool_id = strtolower($order_pool_id);
		    $order_volume = strtolower($order_volume);
		}
		$order = " ORDER BY $order_pool_id ASC, $order_volume ASC ";

		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = " LIMIT $limit_val ";
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}

		$where = Database::getWhere($criteria);

		$sql = 'SELECT Media.*, 
pool1.Name as pool, 
pool2.Name as scratchpool, 
pool3.Name as recyclepool, 
Storage.Name as storage 
FROM Media 
LEFT JOIN Pool AS pool1 USING (PoolId) 
LEFT JOIN Pool AS pool2 ON Media.ScratchPoolId = pool2.PoolId 
LEFT JOIN Pool AS pool3 ON Media.RecyclePoolId = pool3.PoolId 
LEFT JOIN Storage USING (StorageId) 
' . $where['where'] . $order . $limit . $offset;
		$volumes = VolumeRecord::finder()->findAllBySql($sql, $where['params']);
		$this->setExtraVariables($volumes);
		return $volumes;
	}

	/**
	 * Get volume overview.
	 *
	 * @param array $criteria SQL criteria to get volume overview
	 * @param integer $limit_val item limit
	 * @param integer $offset_val offset value
	 * @return array volume overview
	 */
	public function getMediaOverview($criteria = [], $limit_val = 0, $offset_val = 0) {
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = " LIMIT $limit_val ";
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}

		$where = Database::getWhere($criteria);

		// get volume type count
		$sql = 'SELECT
			VolType AS voltype,
			COUNT(1) AS count
			FROM Media
			JOIN Pool USING (PoolId)
			' . $where['where'] . '
			GROUP BY VolType ';

		$statement = Database::runQuery($sql, $where['params']);
		$voltype_count = $statement->fetchAll(PDO::FETCH_ASSOC);

		$expire_query = '';
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
			$expire_query = 'CAST((DATE_PART(\'epoch\', Media.LastWritten) + Media.VolRetention) AS INTEGER)';
		} elseif ($db_params['type'] === Database::MYSQL_TYPE) {
			$expire_query = 'CAST((UNIX_TIMESTAMP(Media.LastWritten) + Media.VolRetention) AS UNSIGNED)';
		} elseif ($db_params['type'] === Database::SQLITE_TYPE) {
			$expire_query = 'CAST((strftime(\'%s\', Media.LastWritten) + Media.VolRetention) AS INTEGER)';
		}

		// get disk volume types (file, dedup and alighed together)
		$vt_disk = $this->getDiskVolTypes();
		$sql = 'SELECT
				VolumeName      AS volumename,
				Pool.Name       AS pool,
				Storage.Name    AS storage,
				VolBytes        AS volbytes,
				VolABytes       AS volabytes,
				Media.VolStatus AS volstatus,
				InChanger       AS inchanger,
				Slot            AS slot,
				CASE 
					WHEN Media.VolStatus IN (\'Full\', \'Used\') THEN ' . $expire_query . '
					ELSE 0
				END expiresin
			FROM Media
			JOIN Storage USING (StorageId)
			JOIN Pool USING (PoolId)
			' .  (!empty($where['where']) ? $where['where'] . ' AND ' : ' WHERE ') . ' VolType IN (' . implode(',', $vt_disk) . ')
			ORDER BY VolStatus ASC, LastWritten DESC' . $limit . $offset;

		$statement = Database::runQuery($sql, $where['params']);
		$voltype_disk = $statement->fetchAll(PDO::FETCH_OBJ);

		// get tape volume types
		$vt_tape = $this->getTapeVolTypes();
		$sql = 'SELECT
				VolumeName      AS volumename,
				Pool.Name       AS pool,
				Storage.Name    AS storage,
				VolBytes        AS volbytes,
				VolABytes       AS volabytes,
				Media.VolStatus AS volstatus,
				InChanger       AS inchanger,
				Slot            AS slot,
				CASE 
					WHEN Media.VolStatus IN (\'Full\', \'Used\') THEN ' . $expire_query . '
					ELSE 0
				END expiresin
			FROM Media
			JOIN Storage USING (StorageId)
			JOIN Pool USING (PoolId)
			' .  (!empty($where['where']) ? $where['where'] . ' AND ' : ' WHERE ') . ' VolType IN (' . implode(',', $vt_tape) . ')
			ORDER BY VolStatus ASC, LastWritten DESC' . $limit . $offset;

		$statement = Database::runQuery($sql, $where['params']);
		$voltype_tape = $statement->fetchAll(PDO::FETCH_OBJ);

		// get cloud volume types
		$vt_cloud = $this->getCloudVolTypes();
		$sql = 'SELECT
				VolumeName      AS volumename,
				Pool.Name       AS pool,
				Storage.Name    AS storage,
				VolBytes        AS volbytes,
				VolABytes       AS volabytes,
				Media.VolStatus AS volstatus,
				InChanger       AS inchanger,
				Slot            AS slot,
				CASE 
					WHEN Media.VolStatus IN (\'Full\', \'Used\') THEN ' . $expire_query . '
					ELSE 0
				END expiresin
			FROM Media
			JOIN Storage USING (StorageId)
			JOIN Pool USING (PoolId)
			' .  (!empty($where['where']) ? $where['where'] . ' AND ' : ' WHERE ') . ' VolType IN (' . implode(',', $vt_cloud) . ')
			ORDER BY VolStatus ASC, LastWritten DESC' . $limit . $offset;

		$statement = Database::runQuery($sql, $where['params']);
		$voltype_cloud = $statement->fetchAll(PDO::FETCH_OBJ);

		$disk_count = $tape_count = $cloud_count = 0;
		for ($i = 0; $i < count($voltype_count); $i++) {
			$count = $voltype_count[$i]['count'];
			$voltype = $voltype_count[$i]['voltype'];
			if (in_array($voltype, $vt_disk)) {
				$disk_count += $count;
			} elseif (in_array($voltype, $vt_tape)) {
				$tape_count += $count;
			} elseif (in_array($voltype, $vt_cloud)) {
				$cloud_count += $count;
			}
		}

		$result = [
			'disk' => [
				'media' => $voltype_disk,
				'count' => $disk_count
			],
			'tape' => [
				'media' => $voltype_tape,
				'count' => $tape_count
			],
			'cloud' => [
				'media' => $voltype_cloud,
				'count' => $cloud_count
			]
		];
		return $result;
	}

	public function getVolumesByPoolId($poolid) {
		$volumes = $this->getVolumes(array(
			'Media.PoolId' => [[
				'vals' => [$poolid],
				'operator' => 'AND'
			]]
		));
		$this->setExtraVariables($volumes);
		return $volumes;
	}

	public function getVolumeByPoolId($poolid) {
		$volume = $this->getVolumes(array(
			'Media.PoolId' => [[
				'vals' => [$poolid],
				'operator' => 'AND'
			]]
		), 1);
		if (is_array($volume) && count($volume) > 0) {
			$volume = array_shift($volume);
		}
		$this->setExtraVariables($volume);
		return $volume;
	}

	public function getVolumeByName($volume_name) {
		$volume = $this->getVolumes(array(
			'Media.VolumeName' => [[
				'vals' => [$volume_name],
				'operator' => 'AND'
			]]
		), 1);
		if (is_array($volume) && count($volume) > 0) {
			$volume = array_shift($volume);
		}
		$this->setExtraVariables($volume);
		return $volume;
	}

	public function getVolumeById($volume_id) {
		$volume = $this->getVolumes(array(
			'Media.MediaId' => [[
				'vals' => [$volume_id],
				'operator' => 'AND'
			]]
		));
		if (is_array($volume) && count($volume) > 0) {
			$volume = array_shift($volume);
		}
		$this->setExtraVariables($volume);
		return $volume;
	}

	private function setExtraVariables(&$volumes) {
		if (is_array($volumes)) {
			foreach($volumes as $volume) {
				$this->setWhenExpire($volume);
			}
		} elseif (is_object($volumes)) {
			$this->setWhenExpire($volumes);
		}
	}

	private function setWhenExpire(&$volume) {
		$volstatus = strtolower($volume->volstatus);
		if ($volstatus == 'full' || $volstatus == 'used') {
			$whenexpire = strtotime($volume->lastwritten) + $volume->volretention;
			$whenexpire = date( 'Y-m-d H:i:s', $whenexpire);
		} else{
			$whenexpire = 'no date';
		}
		$volume->whenexpire = $whenexpire;
	}

	/**
	 * Get volumes for specific jobid and fileid.
	 *
	 * @param integer $jobid job identifier
	 * @param integer $fileid file identifier
	 * @return array volumes list
	 */
	public function getVolumesForJob($jobid, $fileid) {
		$connection = VolumeRecord::finder()->getDbConnection();
		$connection->setActive(true);
		$sql = sprintf('SELECT first_index, last_index, VolumeName AS volname, InChanger AS inchanger FROM (
		 SELECT VolumeName, InChanger, MIN(FirstIndex) as first_index, MAX(LastIndex) as last_index
		 FROM JobMedia JOIN Media ON (JobMedia.MediaId = Media.MediaId)
		 WHERE JobId = %d GROUP BY VolumeName, InChanger
		) AS gv, File
		 WHERE FileIndex >= first_index
		 AND FileIndex <= last_index
		 AND File.FileId = %d', $jobid, $fileid);
		$pdo = $connection->getPdoInstance();
		$result = $pdo->query($sql);
		$ret = $result->fetchAll();
		$pdo = null;
		$volumes = array();
		if (is_array($ret)) {
			for ($i = 0; $i < count($ret); $i++) {
				$volumes[] = array(
					'first_index' => $ret[$i]['first_index'],
					'last_index' => $ret[$i]['last_index'],
					'volume' => $ret[$i]['volname'],
					'inchanger' => $ret[$i]['inchanger']
				);
			}
		}
		return $volumes;
	}

	/**
	 * Get volumes basing on specific criteria and return results as an array
	 * with volume names as keys.
	 *
	 * @param array $criteria array with criterias (@see VolumeManager::getVolumes)
	 * @param integer $limit_val limit results value
	 * @return array volume list with volume names as keys
	 */
	public function getVolumesKeys($criteria = array(), $limit_val = 0) {
		$volumes = [];
		$vols = $this->getVolumes($criteria, $limit_val);
		for ($i = 0; $i < count($vols); $i++) {
			$volumes[$vols[$i]->volumename] = $vols[$i];
		}
		return $volumes;
	}
}
?>
