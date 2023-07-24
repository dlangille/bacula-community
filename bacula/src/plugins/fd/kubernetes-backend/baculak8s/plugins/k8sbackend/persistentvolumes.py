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

import pathlib
from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.plugins.k8sbackend.k8sfileinfo import *
from baculak8s.util.size_util import k8s_size_to_int
from baculak8s.plugins.k8sbackend.k8sutils import *


def persistentvolume_read(corev1api, name):
    return corev1api.read_persistent_volume(name)


def persistentvolumes_list_all(corev1api, pvfilter=None, estimate=False):
    pvlist = {}
    persistentvolumes = corev1api.list_persistent_volume(watch=False)
    for pv in persistentvolumes.items:
        if pvfilter is not None and len(pvfilter) > 0:
            logging.debug("pvfilter-glob-for: {}".format(pv.metadata.name))
            found = False
            for pvglob in pvfilter:
                logging.debug("checking pvglob: {}".format(pvglob))
                if pathlib.Path(pv.metadata.name).match(pvglob):
                    found = True
                    logging.debug('Found.')
                    break
            if not found:
                continue
        pvdata = persistentvolume_read(corev1api, pv.metadata.name)
        spec = encoder_dump(pvdata)
        pvlist[pv.metadata.name] = {
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_PVOLUME,
                              name=pv.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=pvdata.metadata.creation_timestamp),
        }
    return pvlist


def persistentvolumes_names(corev1api):
    pvlist = []
    persistentvolumes = corev1api.list_persistent_volume(watch=False)
    for pv in persistentvolumes.items:
        pvlist.append(["persistentvolume", pv.metadata.name])
    return pvlist


def persistentvolumes_list_all_names(corev1api):
    pvlist = {}
    persistentvolumes = corev1api.list_persistent_volume(watch=False)
    for pv in persistentvolumes.items:
        pvname = pv.metadata.name
        pvsize = pv.spec.capacity['storage']
        logging.debug("pvsize: {} / {}".format(type(pvsize), pvsize))
        pvlist[pvname] = {
            'fi': FileInfo(name="/{}/{}".format(K8SObjType.K8SOBJ_PVOLUME_Path, pvname),
                           ftype=NOT_EMPTY_FILE,
                           size=k8s_size_to_int(pvsize),
                           uid=0, gid=0,
                           mode=DEFAULT_FILE_MODE,
                           nlink=1,
                           modified_at=NOW_TIMESTAMP,
                           accessed_at=NOW_TIMESTAMP,
                           created_at=k8stimestamp_to_unix_timestamp(pv.metadata.creation_timestamp)),
        }
    return pvlist


def persistentvolumes_restore(corev1api, file_info, file_content):
    pv = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(pv.metadata)
    # Instantiate the persistentvolume object
    persistentvolume = client.V1PersistentVolume(
        api_version=pv.api_version,
        kind="PersistentVolume",
        spec=pv.spec,
        metadata=metadata
    )
    if file_info.objcache is not None:
        # object exist so we replace it
        response = corev1api.replace_persistent_volume(k8sfile2objname(file_info.name), persistentvolume, pretty='true')
    else:
        # object does not exist, so create one as required
        response = corev1api.create_persistent_volume(persistentvolume, pretty='true')
    return {'response': response}
