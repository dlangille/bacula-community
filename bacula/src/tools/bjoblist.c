/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

#include "bacula.h"
#include "filed/fd_plugins.h"
#include "findlib/bfile.h"

static const char *working="./";

#define USE_JOB_LIST
#define UNITTESTS
#define MAX_PATH 512

#include "plugins/fd/fd_common.h"

void *start_heap;

int main(int argc, char *argv[]) {
	
	int opt;
	POOL_MEM tmp;
	alist tmp_list(10, owned_by_alist); // define magick number
	BFILE *file;

	char *dat_file = NULL;
	char *key = NULL;
	char *prev_job = NULL;
	char *curr_job = NULL;
	char level;
	char *data = NULL;
	char *search_query = NULL;
	bool is_store = false;

	// Array holding all command line args for easy for loop free memory

	// Parsing of command line arguments
	while ((opt = getopt(argc, argv, "w:d:f:k:p:j:l:D:sSh")) != -1) {
		switch(opt) {
		case 'w': // Working dir
			working_directory = optarg;
			break;


		case 'f': // datfile
			dat_file = optarg;

			break;

		case 'k': // key (hostname + volume name)
			key = optarg;
			break;
		
		case 'p': // previous job
			prev_job = optarg;
			break;
		
		case 'j': // current job
			curr_job = optarg;
			break;

		case 'l': // level 
			level = optarg[0];
			break;
		
		case 'D': // Data
			// TODO is no data put curr job instead
			data = optarg;
			break;
		
		case 's': // Store
			is_store = true;
			break;

		case 'S': // Search
			is_store = false;
			break;

		case 'h': // help default is help as well
		default:
			printf("bjoblist: joblist command line tool : store/search jobs and find deletable ones\n" 
					"USAGE: bjoblist [OPTIONS]\n"
					"Options-styles: -option\n\n"
					"\t-w\tWorking directory\n"
					"\t-d\tDat file with job hierarchy info\n"
					"\t-k\tKey (hostname + volume name)\n"
					"\t-p\tPrevious job\n"
					"\t-j\tCurrent job\n"
					"\t-l\tLevel {(f)ull, (d)ifferential, (i)ncremental}\n"
					"\t-D\tData XXX-XXX-XXX-XXX-XXX\n"
					"\t-s\tStore-mode\n"
					"\t-S\tSearch-mode\n"
					"\t-h\t display this text\n\n");
		}
	}
	
	joblist job_history(NULL, key, curr_job, prev_job, level); // context can be null
	job_history.set_base(dat_file);

	int ret1 = 0;
	if (is_store) {
		ret1 = job_history.store_job(data);
		job_history.prune_jobs(NULL, NULL, &tmp_list);

		char *buf;

		printf("ret1 %d\n", ret1);

		foreach_alist(buf, &tmp_list) {
			printf("%s\n", buf); 
		}
	} else {
		job_history.find_job(curr_job, tmp.handle());
		printf("Search out %s\n", curr_job);
	}

	
	return 1;	
}

