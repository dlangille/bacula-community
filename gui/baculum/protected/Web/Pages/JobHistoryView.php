<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2018 Kern Sibbald
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

Prado::using('System.Web.UI.ActiveControls.TActiveLabel');
Prado::using('System.Web.UI.ActiveControls.TActiveLinkButton');
Prado::using('System.Web.UI.ActiveControls.TCallback');
Prado::using('Application.Web.Class.BaculumWebPage'); 

class JobHistoryView extends BaculumWebPage {

	const IS_RUNNING = 'IsRunning';
	const JOBID = 'JobId';
	const JOB_NAME = 'JobName';
	const JOB_UNAME = 'JobUname';
	const JOB_TYPE = 'JobType';

	const USE_CACHE = true;

	const RESOURCE_SHOW_PATTERN = '/^\s+--> %resource: name=(.+?(?=\s\S+\=.+)|.+$)/i';

	private $jobdata;
	public $is_running = false;
	public $fileset;
	public $schedule;

	public function onInit($param) {
		parent::onInit($param);
		$jobid = 0;
		if ($this->Request->contains('jobid')) {
			$jobid = intval($this->Request['jobid']);
			$this->RunJobModal->setJobId($jobid);
		}
		$jobdata = $this->getModule('api')->get(
			array('jobs', $jobid), null, true, self::USE_CACHE
		);
		$this->jobdata = $jobdata->error === 0 ? $jobdata->output : null;
		$this->RunJobModal->setJobName($this->jobdata->name);
		$this->setJobId($this->jobdata->jobid);
		$this->setJobName($this->jobdata->name);
		$this->setJobUname($this->jobdata->job);
		$this->setJobType($this->jobdata->type);
		$this->is_running = $this->getModule('misc')->isJobRunning($this->jobdata->jobstatus);
	}

	public function onLoad($param) {
		parent::onLoad($param);
		if ($this->IsCallBack || $this->IsPostBack) {
			return;
		}
		$this->refreshJobLog(null, null);
	}

	/**
	 * Set jobid to run job again.
	 *
	 * @return none;
	 */
	public function setJobId($jobid) {
		$jobid = intval($jobid);
		$this->setViewState(self::JOBID, $jobid, 0);
	}

	/**
	 * Get jobid to run job again.
	 *
	 * @return integer jobid
	 */
	public function getJobId() {
		return $this->getViewState(self::JOBID, 0);
	}

	/**
	 * Set job name to run job again.
	 *
	 * @return none;
	 */
	public function setJobName($job_name) {
		$this->setViewState(self::JOB_NAME, $job_name);
	}

	/**
	 * Get job name to run job again.
	 *
	 * @return string job name
	 */
	public function getJobName() {
		return $this->getViewState(self::JOB_NAME);
	}

	/**
	 * Set job uname.
	 *
	 * @return none;
	 */
	public function setJobUname($job_uname) {
		$this->setViewState(self::JOB_UNAME, $job_uname);
	}

	/**
	 * Get job uname.
	 *
	 * @return string job uname
	 */
	public function getJobUname() {
		return $this->getViewState(self::JOB_UNAME);
	}

	/**
	 * Set job type.
	 *
	 * @return none;
	 */
	public function setJobType($job_type) {
		$this->setViewState(self::JOB_TYPE, $job_type);
	}

	/**
	 * Get job type.
	 *
	 * @return string job type
	 */
	public function getJobType() {
		return $this->getViewState(self::JOB_TYPE);
	}

	/**
	 * Refresh job log page and load latest logs.
	 *
	 * @param $sender TActiveLabel sender object
	 * @param $param TCallbackParameter parameter object
	 */
	public function refreshJobLog($sender, $param) {
		$joblog = $this->getModule('api')->get(array('joblog', $this->getJobId()))->output;
		if (!is_array($joblog) || count($joblog) == 0) {
			$msg = Prado::localize("Output for selected job is not available yet or you do not have enabled logging job logs to the catalog database. For watching job the log you need to add to the job Messages resource the following directive: console = all, !skipped, !saved");
			$joblog = array($msg);

		}
		if ($this->is_running) {
			$this->RunningIcon->Display = 'Dynamic';
			$this->FinishedIcon->Display = 'None';
			$this->CancelBtn->Display = 'Dynamic';
			$this->DeleteBtn->Display = 'None';
			$this->RestoreBtn->Display = 'None';
		} else {
			$this->FinishedIcon->Display = 'Dynamic';
			$this->RunningIcon->Display = 'None';
			$this->CancelBtn->Display = 'None';
			$this->DeleteBtn->Display = 'Dynamic';
			$this->RestoreBtn->Display = $this->getJobType() == 'B' ? 'Dynamic' : 'None';
		}
		$this->JobLog->Text = implode(PHP_EOL, array_reverse($joblog));
	}

	public function loadRunJobModal($sender, $param) {
		$this->RunJobModal->loadData();
	}

	public function loadJobConfig($sender, $param) {
		if (!empty($_SESSION['dir'])) {
			$this->JobConfig->setComponentName($_SESSION['dir']);
			$this->JobConfig->setResourceName($this->getJobName());
			$this->JobConfig->setLoadValues(true);
			$this->JobConfig->raiseEvent('OnDirectiveListLoad', $this, null);
		}
	}

	public function getResourceName($resource, $jobshow) {
		$resource_name = null;
		$pattern = str_replace('%resource', $resource, self::RESOURCE_SHOW_PATTERN);
		for ($i = 0; $i < count($jobshow); $i++) {
			if (preg_match($pattern, $jobshow[$i], $match) === 1) {
				$resource_name = $match[1];
				break;
			}
		}
		return $resource_name;
	}

	public function loadFileSetConfig($sender, $param) {
		if (!empty($_SESSION['dir'])) {
			$jobshow = $this->getModule('api')->get(array(
				'jobs', 'show', $this->getJobId()
			))->output;
			$fileset = $this->getResourceName('fileset', $jobshow);
			$this->FileSetConfig->setComponentName($_SESSION['dir']);
			$this->FileSetConfig->setResourceName($fileset);
			$this->FileSetConfig->setLoadValues(true);
			$this->FileSetConfig->raiseEvent('OnDirectiveListLoad', $this, null);
		}
	}

	public function loadScheduleConfig($sender, $param) {
		if (!empty($_SESSION['dir'])) {
			$jobshow = $this->getModule('api')->get(array(
				'jobs', 'show', $this->getJobId()
			))->output;
			$schedule = $this->getResourceName('schedule', $jobshow);
			$this->ScheduleConfig->setComponentName($_SESSION['dir']);
			$this->ScheduleConfig->setResourceName($schedule);
			$this->ScheduleConfig->setLoadValues(true);
			$this->ScheduleConfig->raiseEvent('OnDirectiveListLoad', $this, null);
		}
	}

	public function cancel($sender, $param) {
		$this->getModule('api')->set(array('jobs', 'cancel', $this->getJobId()), array('a' => 'b'));
		$this->refreshJobLog(null, null);
	}

	public function delete($sender, $param) {
		$this->getModule('api')->remove(array('jobs', $this->getJobId()));
	}
}
?>