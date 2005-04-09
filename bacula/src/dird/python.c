/*
 *
 * Bacula interface to Python for the Director
 *
 * Kern Sibbald, November MMIV
 *
 *   Version $Id$
 *
 */

/*
   Copyright (C) 2000-2005 Kern Sibbald

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
#include "dird.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

extern JCR *get_jcr_from_PyObject(PyObject *self);

PyObject *bacula_get(PyObject *self, PyObject *args);
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw);
PyObject *bacula_run(PyObject *self, PyObject *args);

/* Define Bacula entry points */
PyMethodDef BaculaMethods[] = {
    {"get", bacula_get, METH_VARARGS, "Get Bacula variables."},
    {"set", (PyCFunction)bacula_set, METH_VARARGS|METH_KEYWORDS,
        "Set Bacula variables."},
    {"run", (PyCFunction)bacula_run, METH_VARARGS, "Run a Bacula command."},
    {NULL, NULL, 0, NULL}             /* last item */
};


struct s_vars {
   const char *name;
   char *fmt;
};

static struct s_vars vars[] = {
   { N_("Job"),        "s"},
   { N_("Dir"),        "s"},
   { N_("Level"),      "s"},
   { N_("Type"),       "s"},
   { N_("JobId"),      "i"},
   { N_("Client"),     "s"},
   { N_("NumVols"),    "i"},
   { N_("Pool"),       "s"},
   { N_("Storage"),    "s"},
   { N_("Catalog"),    "s"},
   { N_("MediaType"),  "s"},
   { N_("JobName"),    "s"},
   { N_("JobStatus"),  "s"},

   { NULL,             NULL}
};

/* Return Bacula variables */
PyObject *bacula_get(PyObject *self, PyObject *args)
{
   JCR *jcr;
   char *item;
   bool found = false;
   int i;
   char buf[10];

   if (!PyArg_ParseTuple(args, "s:get", &item)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   for (i=0; vars[i].name; i++) {
      if (strcmp(vars[i].name, item) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      return NULL;
   }
   switch (i) {
   case 0:                            /* Job */
      return Py_BuildValue(vars[i].fmt, jcr->job->hdr.name);
   case 1:                            /* Director's name */
      return Py_BuildValue(vars[i].fmt, my_name);
   case 2:                            /* level */
      return Py_BuildValue(vars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 3:                            /* type */
      return Py_BuildValue(vars[i].fmt, job_type_to_str(jcr->JobType));
   case 4:                            /* JobId */
      return Py_BuildValue(vars[i].fmt, jcr->JobId);
   case 5:                            /* Client */
      return Py_BuildValue(vars[i].fmt, jcr->client->hdr.name);
   case 6:                            /* NumVols */
      return Py_BuildValue(vars[i].fmt, jcr->NumVols);
   case 7:                            /* Pool */
      return Py_BuildValue(vars[i].fmt, jcr->pool->hdr.name);
   case 8:                            /* Storage */
      return Py_BuildValue(vars[i].fmt, jcr->store->hdr.name);
   case 9:
      return Py_BuildValue(vars[i].fmt, jcr->catalog->hdr.name);
   case 10:                           /* MediaType */
      return Py_BuildValue(vars[i].fmt, jcr->store->media_type);
   case 11:                           /* JobName */
      return Py_BuildValue(vars[i].fmt, jcr->Job);
   case 12:                           /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue(vars[i].fmt, buf);
   }
   return NULL;
}

/* Set Bacula variables */
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw)
{
   JCR *jcr;
   char *msg = NULL;
   char *VolumeName = NULL;
   static char *kwlist[] = {"JobReport", "VolumeName", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "|ss:set", kwlist,
        &msg, &VolumeName)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);

   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
   }
   if (VolumeName) {
      if (is_volume_name_legal(NULL, VolumeName)) {
         pm_strcpy(jcr->VolumeName, VolumeName);
      } else {
         return Py_BuildValue("i", 0);  /* invalid volume name */
      }
   }
   return Py_BuildValue("i", 1);
}

/* Run a Bacula command */
PyObject *bacula_run(PyObject *self, PyObject *args)
{
   JCR *jcr;
   char *item;
   int stat;

   if (!PyArg_ParseTuple(args, "s:get", &item)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   pm_strcpy(ua->cmd, item);          /* copy command */
   parse_ua_args(ua);                 /* parse command */
   stat = run_cmd(ua, ua->cmd);
   free_ua_context(ua);
   return Py_BuildValue("i", stat);
}


#endif /* HAVE_PYTHON */
