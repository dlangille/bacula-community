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
 * ACLs for Bacula configuration part.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class BaculaConfigACL extends APIModule
{
	/**
	 * Special config ACL action names.
	 */
	private static $config_acl_actions = [
		'READ',
		'CREATE',
		'UPDATE',
		'DELETE'
	];
	/**
	 * Validate if request command is allowed.
	 *
	 * @param string $console_name Director Console name
	 * @param string $action current action (@see BaculaConfigACL::$config_acl_actions)
	 * @param string $component_type component type (currently not used)
	 * @param string $resource_type resource type
	 * @return bool true if request command is allowed, false otherwise
	 */
	public function validateCommand($user_id, $action, $component_type, $resource_type)
	{
		$valid = false;
		$resource = strtoupper($resource_type);
		if ($this->validateAction($action)) {
			$command_acls = $this->getCommandACLs($user_id);
			for ($i = 0; $i < count($command_acls); $i++) {
				if ($command_acls[$i]['action'] === $action && $command_acls[$i]['keyword'] === $resource) {
					$valid = true;
					break;
				}
			}
		}
		return $valid;
	}

	/**
	 * Validate action.
	 *
	 * @param string $action action name (ex. 'READ' or 'DELETE')
	 * @return bool true if action is valid, false otherwise
	 */
	private function validateAction($action)
	{
		return in_array($action, self::$config_acl_actions);
	}

	/**
	 * Get special commands for restrictions from Director Console resource.
	 *
	 * @param $console_name console name
	 * @return array commands for restrictions or empty array if no command found
	 */
	private function getCommandACLs($console_name)
	{
		$command_acls = [];
		$bacula_setting = $this->getModule('bacula_setting');
		$config = $bacula_setting->getConfig(
			'dir',
			'Console',
			$console_name
		);
		if ($config['exitcode'] === 0) {
			if (key_exists('CommandAcl', $config['output'])) {
				$command_acls = $this->findCommands($config['output']['CommandAcl']);
			}
		}
		return $command_acls;
	}

	/**
	 * Find special command ACLs that defines config restrictions.
	 * It takes CommandACLs directive value and reads it to find commands.
	 *
	 * @param array $commands CommandACLs command list
	 * @return array restriction commands or empty array if no command found
	 */
	private function findCommands(array $commands)
	{
		$command_acls = [];
		for ($i = 0; $i < count($commands); $i++) {
			// @TODO: Propose using commands in form <RESOURCE>_<ACTION> or <PREFIX>_<RESOURCE>_<ACTION>
			if (preg_match('/^(?P<action>(READ|CREATE|UPDATE|DELETE))_(?P<keyword>[A-Z]+)$/', $commands[$i], $match) === 1) {
				$command_acls[] = [
					'keyword' => $match['keyword'],
					'action' => $match['action']
				];
			}
		}
		return $command_acls;
	}

}
?>
