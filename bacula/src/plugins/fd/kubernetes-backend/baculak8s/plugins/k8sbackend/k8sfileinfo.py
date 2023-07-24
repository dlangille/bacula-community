# -*- coding: UTF-8 -*-
#
#  Bacula(R) - The Network Backup Solution
#
#   Copyright (C) 2000-2022 Kern Sibbald
#
#   The original author of Bacula is Kern Sibbald, with contributions
#   from many others, a complete list can be found in the file AUTHORS.
#
#   You may use this file and others of this release according to the
#   license defined in the LICENSE file, which includes the Affero General
#   Public License, v3.0 ("AGPLv3") and some additional permissions and
#   terms pursuant to its AGPLv3 Section 7.
#
#   This notice must be preserved when any source code is
#   conveyed and/or propagated.
#
#   Bacula(R) is a registered trademark of Kern Sibbald.
#
#     Copyright (c) 2019 by Inteos sp. z o.o.
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

import json
import logging

import yaml
from baculak8s.entities.file_info import (DEFAULT_DIR_MODE, DEFAULT_FILE_MODE,
                                          DIRECTORY, FileInfo)
from baculak8s.entities.k8sobjtype import K8SObjType
from baculak8s.util.date_util import (get_time_now,
                                      k8stimestamp_to_unix_timestamp)
from baculak8s.util.size_util import k8s_size_to_int

NOW_TIMESTAMP = get_time_now()
defaultk8sext = 'yaml'
defaultk8spath = '@kubernetes'
defaultk8sarchext = 'tar'


def encoder_dump(msg):
    if defaultk8sext == 'json':
        return json.dumps(msg, sort_keys=True, default=str)
    else:
        return yaml.dump(msg)


def encoder_load(msg, filename=None):
    if filename.endswith('.json') or (filename is None and defaultk8sext == 'json'):
        return json.loads(msg)
    else:
        return yaml.load(msg, Loader=yaml.Loader)


def k8sfile2objname(fname):
    return str(fname).replace('.'+defaultk8sext, '').replace('.json', '').replace('.yaml', '').replace('.tar', '')


def k8sfilepath(objtype, nsname='', name=''):
    # TODO: refactor string format to new ".format()" method
    if objtype is not None:
        if objtype == K8SObjType.K8SOBJ_NAMESPACE:
            # namespace
            return '/%s/%s/%s/%s.%s' % (defaultk8spath, K8SObjType.pathdict[K8SObjType.K8SOBJ_NAMESPACE],
                                        nsname, name, defaultk8sext)
        if objtype == K8SObjType.K8SOBJ_PVOLUME:
            # persistent volume
            return '/%s/%s/%s.%s' % (defaultk8spath, K8SObjType.pathdict[K8SObjType.K8SOBJ_PVOLUME],
                                     name, defaultk8sext)
        if objtype == K8SObjType.K8SOBJ_STORAGECLASS:
            # storage class
            return '/%s/%s/%s.%s' % (defaultk8spath, K8SObjType.pathdict[K8SObjType.K8SOBJ_STORAGECLASS],
                                     name, defaultk8sext)
        if objtype == K8SObjType.K8SOBJ_PVCDATA:
            # PVC Data tar archive here
            return '/%s/%s/%s/%s/%s.%s' % (defaultk8spath, K8SObjType.pathdict[K8SObjType.K8SOBJ_NAMESPACE],
                                           nsname, K8SObjType.pathdict[K8SObjType.K8SOBJ_PVCDATA],
                                           name, defaultk8sarchext)
        # other objects
        return '/%s/%s/%s/%s/%s.%s' % (defaultk8spath, K8SObjType.pathdict[K8SObjType.K8SOBJ_NAMESPACE],
                                       nsname, K8SObjType.pathdict[objtype], name, defaultk8sext)
    return None


def k8sfileinfo(objtype, name, ftype, size, nsname=None, creation_timestamp=None):
    return FileInfo(
        name=k8sfilepath(objtype, nsname=nsname, name=name),
        ftype=ftype,
        size=k8s_size_to_int(size),
        objtype=objtype,
        uid=0, gid=0,
        mode=DEFAULT_DIR_MODE if ftype == DIRECTORY else DEFAULT_FILE_MODE,
        # TODO: Persistent volumes can have a different access modes [RWO, ROM, RWM]
        # TODO: which we can express as different file modes in Bacula
        nlink=1,
        modified_at=NOW_TIMESTAMP,
        accessed_at=NOW_TIMESTAMP,
        created_at=NOW_TIMESTAMP if creation_timestamp is None else k8stimestamp_to_unix_timestamp(creation_timestamp)
    )


def k8sfileobjecttype(fnames):
    if len(fnames) < 3 or fnames[0] != defaultk8spath:
        # the filepath variable cannot be converted to k8s fileinfo
        return None
    objtype = {
        'obj': None,
        'namespace': None,
    }
    if fnames[1] == K8SObjType.K8SOBJ_NAMESPACE_Path:
        # handle namespaced objects
        objtype.update({'namespace': fnames[2]})
        filename = fnames[3]
        if filename.endswith('.%s' % defaultk8sext):
            objtype.update({'obj': K8SObjType.K8SOBJ_NAMESPACE})
        elif filename == K8SObjType.K8SOBJ_PVCS_Path:
            # handle pvcs both config and data
            filename = fnames[4]
            if filename.endswith('.%s' % defaultk8sext):
                # this is a config file
                objtype.update({'obj': K8SObjType.K8SOBJ_PVOLCLAIM})
            else:
                # any other are pvcdata files
                objtype.update({'obj': K8SObjType.K8SOBJ_PVCDATA})
        else:
            for obj in K8SObjType.pathdict.keys():
                if K8SObjType.pathdict[obj] == filename:
                    objtype.update({'obj': obj})
                    break
    elif fnames[1] == K8SObjType.K8SOBJ_PVOLUME_Path:
        # handle persistent volumes here
        objtype.update({'obj': K8SObjType.K8SOBJ_PVOLUME})
    elif fnames[1] == K8SObjType.K8SOBJ_STORAGECLASS_Path:
        # handle storage class here
        objtype.update({'obj': K8SObjType.K8SOBJ_STORAGECLASS})
    logging.debug('objtype:' + str(objtype))
    return objtype
