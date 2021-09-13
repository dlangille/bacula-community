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

int main(int argc, char *argv[]) {
	
	int opt;

	while ((opt = getopt(argc, argv, "wdkpjlDmh")) != -1) {
		switch(opt) {
		case 'w': // Working dir
			printf("w\n");
			break;

		case 'd': // datfile
			printf("d\n");
			break;

		case 'k': // key (hostname + volume name)
			printf("w\n");
			break;
		
		case 'p': // previous job
			printf("p\n");
			break;
		
		case 'j': // current job
			printf("j\n");
			break;

		case 'l': // level 
			printf("l\n");
			break;
		
		case 'D': // Data
			printf("D\n");
			break;
		
		case 'm': // mode {store, search}
			printf("m\n");
			break;

		case 'h': // help default is help as well
		default:
			printf("bjoblist: joblist command line tool : store/search jobs and find deletable ones\n" 
					"USAGE: bjoblist [OPTIONS]\n"
					"Options-styles: -option\n\n"
					"\t-w\tWoking directory\n"
					"\t-d\tDat file with job hierarchy info\n"
					"\t-k\tKey (hostname + volume name)\n"
					"\t-p\tPrevious job\n"
					"\t-j\tCurrent job\n"
					"\t-l\tLevel {(f)ull, (d)ifferential, (i)ncremental}\n"
					"\t-D\tData XXX-XXX-XXX-XXX-XXX\n"
					"\t-m\tMode {store, search}\n"
					"\t-h\t display this text\n\n");
		}
	}
}
