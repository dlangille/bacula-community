/*
 * Windows APIs that are different for each system.
 *   We use pointers to the entry points so that a
 *   single binary will run on all Windows systems.
 *
 *     Kern Sibbald MMIII
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "find.h"

#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)

/* API Pointers */

t_OpenProcessToken	p_OpenProcessToken = NULL;
t_AdjustTokenPrivileges p_AdjustTokenPrivileges = NULL;
t_LookupPrivilegeValue	p_LookupPrivilegeValue = NULL;

t_GetFileAttributesExA	p_GetFileAttributesExA = NULL;
t_GetFileAttributesExW	p_GetFileAttributesExW = NULL;
t_BackupRead		p_BackupRead = NULL;
t_BackupWrite		p_BackupWrite = NULL;
t_SetProcessShutdownParameters p_SetProcessShutdownParameters = NULL;

#endif
