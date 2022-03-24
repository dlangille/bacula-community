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

import logging
import time
from abc import ABCMeta

import yaml
from baculak8s.jobs.job import Job
from baculak8s.plugins.k8sbackend.baculabackup import (BACULABACKUPIMAGE,
                                                       BACULABACKUPPODNAME,
                                                       ImagePullPolicy,
                                                       prepare_backup_pod_yaml)
from baculak8s.plugins.k8sbackend.pvcclone import prepare_backup_clone_yaml
from baculak8s.util.respbody import parse_json_descr
from baculak8s.util.sslserver import DEFAULTTIMEOUT, ConnectionServer
from baculak8s.util.token import generate_token

DEFAULTRECVBUFFERSIZE = 64 * 1048
PLUGINHOST_NONE_ERR = "PLUGINHOST parameter is missing and cannot be autodetected. " \
                      "Cannot continue with pvcdata backup!"
POD_EXECUTION_ERR = "Cannot successfully start bacula-backup pod in expected time!"
POD_REMOVE_ERR = "Unable to remove proxy Pod {podname}! Other operations with proxy Pod will fail!"
POD_EXIST_ERR = "Job already running in '{namespace}' namespace. Check logs or delete {podname} Pod manually."
TAR_STDERR_UNKNOWN = "Unknown error. You should check Pod logs for possible explanation."
PLUGINPORT_VALUE_ERR = "Cannot use provided pluginport={port} option. Used default!"
FDPORT_VALUE_ERR = "Cannot use provided fdport={port} option. Used default!"
POD_YAML_PREPARED_INFO = "Prepare backup Pod with: {image} <{pullpolicy}> {pluginhost}:{pluginport}"
POD_YAML_PREPARED_INFO_NODE = "Prepare Bacula Pod on: {nodename} with: {image} <{pullpolicy}> {pluginhost}:{pluginport}"
CANNOT_CREATE_BACKUP_POD_ERR = "Cannot create backup pod. Err={}"
CANNOT_REMOVE_BACKUP_POD_ERR = "Cannot remove backup pod. Err={}"
PVCDATA_GET_ERROR = "Cannot get PVC Data object Err={}"
PVCCLONE_YAML_PREPARED_INFO = "Prepare snapshot: {namespace}/{snapname} storage: {storage} capacity: {capacity}"
CANNOT_CREATE_PVC_CLONE_ERR = "Cannot create PVC snapshot. Err={}"
CANNOT_REMOVE_PVC_CLONE_ERR = "Cannot remove PVC snapshot. Err={}"
CANNOT_START_CONNECTIONSERVER = "Cannot start ConnectionServer. Err={}"


class JobPodBacula(Job, metaclass=ABCMeta):
    """

    This is a common class for all job which handles pvcdata backups using
    bacula-backup Pod.

    """

    def __init__(self, plugin, io, params):
        super().__init__(plugin, io, params)
        self.connsrv = None
        self.fdaddr = params.get("fdaddress")
        self.fdport = params.get("fdport", 9104)
        self.pluginhost = params.get("pluginhost", self.fdaddr)
        self.pluginport = params.get("pluginport", self.fdport)
        self.certfile = params.get('fdcertfile')
        _keyfile = params.get('fdkeyfile')
        self.keyfile = _keyfile if _keyfile is not None else self.certfile
        self._prepare_err = False
        self.token = None
        self.jobname = '{name}:{jobid}'.format(name=params.get('name', 'undefined'), jobid=params.get('jobid', '0'))
        self.timeout = params.get('timeout', DEFAULTTIMEOUT)
        try:
            self.timeout = int(self.timeout)
        except ValueError:
            self.timeout = DEFAULTTIMEOUT
        self.timeout = max(1, self.timeout)
        self.tarstderr = ''
        self.tarexitcode = None
        self.backupimage = params.get('baculaimage', BACULABACKUPIMAGE)
        self.imagepullpolicy = ImagePullPolicy.process_param(params.get('imagepullpolicy'))

    def handle_pod_logs(self, connstream):
        logmode = ''
        self.tarstderr = ''
        with connstream.makefile(mode='r') as fd:
            self.tarexitcode = fd.readline().strip()
            logging.debug('handle_pod_logs:tarexitcode:{}'.format(self.tarexitcode))
            while True:
                data = fd.readline()
                if not data:
                    break
                logging.debug('LOGS:{}'.format(data.strip()))
                if data.startswith('---- stderr ----'):
                    logmode = 'stderr'
                    continue
                elif data.startswith('---- list ----'):
                    logmode = 'list'
                    continue
                elif data.startswith('---- end ----'):
                    break
                if logmode == 'stderr':
                    self.tarstderr += data
                    continue
                elif logmode == 'list':
                    # no listing feature yet
                    continue

    def handle_pod_data_recv(self, connstream):
        while True:
            data = self.connsrv.streamrecv(DEFAULTRECVBUFFERSIZE)
            if not data:
                logging.debug('handle_pod_data_recv:EOT')
                break
            logging.debug('handle_pod_data_recv:D' + str(len(data)))
            self._io.send_data(data)

    def handle_pod_data_send(self, connstream):
        while True:
            data = self._io.read_data()
            if not data:
                logging.debug('handle_pod_data_send:EOT')
                break
            self.connsrv.streamsend(data)
            logging.debug('handle_pod_data_send:D{}'.format(len(data)))

    def prepare_pod_yaml(self, namespace, pvcdata, mode='backup'):
        logging.debug('pvcdata: {}'.format(pvcdata))
        if self.pluginhost is None:
            self._handle_error(PLUGINHOST_NONE_ERR)
            self._prepare_err = True
            return None
        pport = self.pluginport
        try:
            self.pluginport = int(self.pluginport)
        except ValueError:
            self.pluginport = 9104
            logging.warning(PLUGINPORT_VALUE_ERR.format(port=pport))
            self._io.send_warning(PLUGINPORT_VALUE_ERR.format(port=pport))
        pvcname = pvcdata.get('name')
        node_name = pvcdata.get('node_name')

        podyaml = prepare_backup_pod_yaml(mode=mode, nodename=node_name, host=self.pluginhost, port=self.pluginport,
                                          token=self.token, namespace=namespace, pvcname=pvcname, image=self.backupimage,
                                          imagepullpolicy=self.imagepullpolicy, job=self.jobname)
        if node_name is None:
            self._io.send_info(POD_YAML_PREPARED_INFO.format(
                image=self.backupimage,
                pullpolicy=self.imagepullpolicy,
                pluginhost=self.pluginhost,
                pluginport=self.pluginport
            ))
        else:
            self._io.send_info(POD_YAML_PREPARED_INFO_NODE.format(
                nodename=node_name,
                image=self.backupimage,
                pullpolicy=self.imagepullpolicy,
                pluginhost=self.pluginhost,
                pluginport=self.pluginport
            ))
        return podyaml

    def prepare_clone_yaml(self, namespace, pvcname, capacity, storage_class):
        logging.debug('prepare_clone_yaml: {} {} {} {}'.format(namespace, pvcname, capacity, storage_class))
        if namespace is None or pvcname is None or capacity is None or storage_class is None:
            logging.error("Invalid params to pvc clone!")
            return None, None
        pvcyaml, snapname = prepare_backup_clone_yaml(namespace, pvcname, capacity, storage_class)
        self._io.send_info(PVCCLONE_YAML_PREPARED_INFO.format(
            namespace=namespace,
            snapname=snapname,
            storage=storage_class,
            capacity=capacity
        ))
        return pvcyaml, snapname

    def prepare_connection_server(self):
        if self.connsrv is None:
            if self.fdaddr is None:
                self.fdaddr = '0.0.0.0'
            fport = self.fdport
            try:
                self.fdport = int(self.fdport)
            except ValueError:
                self.fdport = 9104
                logging.warning(FDPORT_VALUE_ERR.format(port=fport))
                self._handle_error(FDPORT_VALUE_ERR.format(port=fport))
            logging.debug("prepare_connection_server:New ConnectionServer: {}:{}".format(
                str(self.fdaddr),
                str(self.fdport)))
            self.connsrv = ConnectionServer(self.fdaddr, self.fdport,
                                            token=self.token,
                                            certfile=self.certfile,
                                            keyfile=self.keyfile,
                                            timeout=self.timeout)
            response = self.connsrv.listen()
            if isinstance(response, dict) and 'error' in response:
                logging.debug("RESPONSE:{}".format(response))
                self._handle_error(CANNOT_START_CONNECTIONSERVER.format(parse_json_descr(response)))
                return False
        else:
            logging.debug("prepare_connection_server:Reusing ConnectionServer!")
            self.connsrv.token = self.token
        return True

    def execute_pod(self, namespace, podyaml):
        exist = self._plugin.check_pod(namespace=namespace, name=BACULABACKUPPODNAME)
        if exist is not None:
            logging.debug('execute_pod:exist!')
            response = False
            for a in range(self.timeout):
                time.sleep(1)
                response = self._plugin.check_gone_backup_pod(namespace)
                if isinstance(response, dict) and 'error' in response:
                    self._handle_error(CANNOT_REMOVE_BACKUP_POD_ERR.format(parse_json_descr(response)))
                    return False
                else:
                    if response:
                        break
            if not response:
                self._handle_error(POD_EXIST_ERR.format(namespace=namespace, podname=BACULABACKUPPODNAME))
                return False

        poddata = yaml.safe_load(podyaml)
        response = self._plugin.create_backup_pod(namespace, poddata)
        if isinstance(response, dict) and 'error' in response:
            self._handle_error(CANNOT_CREATE_BACKUP_POD_ERR.format(parse_json_descr(response)))
        else:
            for seq in range(self.timeout):
                time.sleep(1)
                isready = self._plugin.backup_pod_isready(namespace, seq)
                if isinstance(isready, dict) and 'error' in isready:
                    self._handle_error(CANNOT_CREATE_BACKUP_POD_ERR.format(parse_json_descr(isready)))
                    break
                elif isready:
                    return True
        return False

    def execute_pvcclone(self, namespace, clonename, cloneyaml):
        pass

    def delete_pod(self, namespace, force=False):
        for a in range(self.timeout):
            time.sleep(1)
            response = self._plugin.check_gone_backup_pod(namespace, force=force)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_REMOVE_BACKUP_POD_ERR.format(parse_json_descr(response)))
            else:
                logging.debug('delete_pod:isgone:{}'.format(response))
                if response:
                    return True
        return False

    def delete_pvcclone(self, namespace, clonename, force=False):
        for a in range(self.timeout):
            time.sleep(1)
            response = self._plugin.check_gone_pvcclone(namespace, clonename, force=force)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_REMOVE_PVC_CLONE_ERR.format(parse_json_descr(response)))
            else:
                logging.debug('delete_pvcclone:isgone:{}'.format(response))
                if response:
                    return True
        return False

    def handle_delete_pod(self, namespace):
        if not self.delete_pod(namespace=namespace):
            self._handle_error(POD_REMOVE_ERR.format(podname=BACULABACKUPPODNAME))

    def handle_tarstderr(self):
        if self.tarexitcode != '0' or len(self.tarstderr) > 0:
            # format or prepare error message
            if not len(self.tarstderr):
                self.tarstderr = TAR_STDERR_UNKNOWN
            else:
                self.tarstderr = self.tarstderr.rstrip('\n')
            # classify it as error or warning
            if self.tarexitcode != '0':
                self._handle_error(self.tarstderr)
            else:
                self._io.send_warning(self.tarstderr)

    def prepare_bacula_pod(self, pvcdata, namespace=None, mode='backup'):
        if self._prepare_err:
            # first prepare yaml was unsuccessful, we can't recover from this error
            return False
        self.token = generate_token()
        if namespace is None:
            namespace = pvcdata.get('fi').namespace
        logging.debug('prepare_bacula_pod:token={} namespace={}'.format(self.token, namespace))
        podyaml = self.prepare_pod_yaml(namespace, pvcdata, mode=mode)
        if podyaml is None:
            # error preparing yaml
            self._prepare_err = True
            return False
        if not self.prepare_connection_server():
            self._prepare_err = True
            return False
        logging.debug('prepare_bacula_pod:start pod')
        if not self.execute_pod(namespace, podyaml):
            self._handle_error(POD_EXECUTION_ERR)
            return False
        return True

    def create_pvcclone(self, namespace, pvcname):
        clonename = None
        logging.debug("pvcclone for:{}/{}".format(namespace, pvcname))
        pvcdata = self._plugin.get_pvcdata_namespaced(namespace, pvcname)
        if isinstance(pvcdata, dict) and 'exception' in pvcdata:
            self._handle_error(PVCDATA_GET_ERROR.format(parse_json_descr(pvcdata)))
        else:
            logging.debug('PVCDATA_ORIG:{}:{}'.format(pvcname, pvcdata))
            cloneyaml, clonename = self.prepare_clone_yaml(namespace, pvcname, pvcdata.get('capacity'), pvcdata.get('storage_class_name'))
            if cloneyaml is None or clonename is None:
                # error preparing yaml
                self._prepare_err = True
                return None
            clonedata = yaml.safe_load(cloneyaml)
            response = self._plugin.create_pvc_clone(namespace, clonedata)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_CREATE_PVC_CLONE_ERR.format(parse_json_descr(response)))
                return None

        return clonename
