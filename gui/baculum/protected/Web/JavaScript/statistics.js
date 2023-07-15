var Statistics = {
	jobs: null,
	clients: null,
	pools: null,
	jobtotals: null,
	dbsize: null,
	jobs_summary: [],
	grab_statistics: function(data, opts) {
		this.jobs = data.jobs;
		this.clients = data.clients;
		this.pools = data.pools;
		this.jobtotals = data.jobtotals;
		this.dbsize = data.dbsize;
		var jobs_count = this.jobs.length;
		var jobs_summary = {
			ok: [],
			error: [],
			warning: [],
			cancel: [],
			running: []
		};
		var status_type;
		const start_time = new Date(Date.now() - (opts.job_age * 1000));
		const start_time_ts = start_time.getTime();
		let job_time_ts;
		for (var i = 0; i < jobs_count; i++) {
			job_time_ts = iso_date_to_timestamp(this.jobs[i].starttime);
			if (opts.job_age > 0 && job_time_ts < start_time_ts) {
				continue;
			}
			if (opts.job_states.hasOwnProperty(this.jobs[i].jobstatus)) {
				status_type = opts.job_states[this.jobs[i].jobstatus].type;
				if (status_type == 'ok' && this.jobs[i].joberrors > 0) {
					status_type = 'warning';
				}
				if (status_type == 'waiting') {
					status_type = 'running';
				}
				jobs_summary[status_type].push(this.jobs[i]);
			}
		}

		this.jobs_summary = jobs_summary;
	}
}
