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
#define COMMAND_LINE_TOOL // See in src/plugin/fd/fd_common.h
#define MAX_RETURN_PRUNE 10

#include "plugins/fd/fd_common.h"

void *start_heap;

int main(int argc, char *argv[]) 
{
	int opt;
	POOLMEM *tmp = get_pool_memory(PM_FNAME);
	alist tmp_list(MAX_RETURN_PRUNE, owned_by_alist); 
	bool store_return = true;
	bool search_return = true;

	// Args to be received by the command line
	char *dat_file = NULL;
	char *key = NULL;
	char *prev_job = NULL;
	char *curr_job = NULL;
	char level = 0;
	char *data = NULL;
	bool is_store = false;
	bool is_search = false;
	debug_level = 0;


	// Parsing of command line arguments
	while ((opt = getopt(argc, argv, "w:f:k:p:j:l:D:sSd:h")) != -1) {
		switch(opt) {
		case 'w': // Working dir
			working_directory = optarg; // global var
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
			is_search = true;
			break;

		case 'd': // Debug-level
			debug_level = atoi(optarg); // global var
			break;

		case 'h': // help default is help as well
		default:
			printf("bjoblist: joblist command line tool : store/search jobs and find deletable ones\n" 
					"USAGE: bjoblist [OPTIONS]\n"
					"Options-styles: -option\n\n"
					"\t-w\tWorking directory\n"
					"\t-f\tDat file with job hierarchy info\n"
					"\t-k\tKey (hostname + volume name)\n"
					"\t-p\tPrevious job\n"
					"\t-j\tCurrent job\n"
					"\t-l\tLevel {(F)ull, (D)ifferential, (I)ncremental}\n"
					"\t-D\tData XXX-XXX-XXX-XXX-XXX\n"
					"\t-s\tStore-mode\n"
					"\t-S\tSearch-mode\n"
					"\t-d\tDebug-level\n"
					"\t-h\t display this text\n\n");
			
			exit(EXIT_SUCCESS);
		}
	}
	
	// Check for correctly initialized variables
	// Prev job can eventually be NULL
	if (working_directory == NULL) {
		printf("Working directory not set EXIT\n");
		exit(EXIT_FAILURE);
	}

	// Check if data_file has been initilaized
	if (dat_file == NULL) {
		printf("Data file not set EXIT\n");
		exit(EXIT_FAILURE);
	}

	// Check that current job is not null
	if (curr_job == NULL) {
		printf("Current job not sell EXIT\n");
		exit(EXIT_FAILURE);
	}

	// Check if key = either F/I/D
	if (level != 'F' && level != 'I' && level != 'D') {
		printf("Bad key has to be either F/I/D\n");
		exit(EXIT_FAILURE);
	}

	// If data is null set it to current job
	if (data == NULL) {
		data = curr_job;
	}

	// Check if there is a search and or store to do
	if (!is_store && !is_search) {
		printf("Store or search not called. Nothing to do EXIT\n");
		exit(EXIT_SUCCESS);
	}

	joblist job_history(NULL, key, curr_job, prev_job, level); // context can be null
	job_history.set_base(dat_file);

	if (is_store) {
		store_return = job_history.store_job(data); // return boolean
		job_history.prune_jobs(NULL, NULL, &tmp_list); // retrun void

		// print all deletable jobs
		char *buf;
		foreach_alist(buf, &tmp_list) {
			printf("%s\n", buf); 
		}
	} 
	
	if (is_search) {
		search_return = job_history.find_job(curr_job, &tmp); // return bool
		printf("Search out %s\n", tmp);
	}

	// Check that search/store op returned correctly (true if not called)
	if (store_return && search_return) { 
		exit(EXIT_SUCCESS);
	} else {
		printf("Store return : %d, Search return %d\n", store_return, search_return);
		exit(EXIT_FAILURE);
	}	
}

