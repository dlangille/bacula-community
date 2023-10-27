# -*- coding: UTF-8 -*-
#
# Bacula(R) - The Network Backup Solution
#
# Copyright (C) 2000-2020 Kern Sibbald
#
# The original author of Bacula is Kern Sibbald, with contributions
# from many others, a complete list can be found in the file AUTHORS.
#
# You may use this file and others of this release according to the
# license defined in the LICENSE file, which includes the Affero General
# Public License, v3.0 ("AGPLv3") and some additional permissions and
# terms pursuant to its AGPLv3 Section 7.
#
# This notice must be preserved when any source code is
# conveyed and/or propagated.
#
# Bacula(R) is a registered trademark of Kern Sibbald.
#
#     Author: Radoslaw Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

from __future__ import unicode_literals
import os
import sys
import ssl
import socket
import time
import logging
import subprocess

COPYRIGHT = 'Copyright (C) 2000-2022 Kern Sibbald'
VERSION = '1.0-rpk'
TARCMD = '/tar'
TARPIPE = '/tmp/baculatar.fifo'
TARSTDOUT = '/tmp/baculatar.stdout'
TARSTDERR = '/tmp/baculatar.stderr'
CONNRETRIES = 600
CONNTIMEOUT = 600
DEFAULTPORT = 9104
ERR_NO_DIR_FOUND = "No /{dir}/ directory found. Cannot execute backup!"


class BaculaConnection(object):

    def __init__(self, *args, **kwargs):
        super(BaculaConnection, self).__init__(*args, **kwargs)
        self.mode = os.getenv('PLUGINMODE', 'backup')
        if self.mode not in ('backup', 'restore'):
            self.mode = 'backup'
        self.host = os.getenv('PLUGINHOST', 'localhost')
        self.port = os.getenv('PLUGINPORT', DEFAULTPORT)
        self.token = os.getenv('PLUGINTOKEN', '')
        self.jobname = os.getenv('PLUGINJOB', 'undefined')
        self.connretries = os.getenv('PLUGINCONNRETRIES', CONNRETRIES)
        if isinstance(self.connretries, str):
            try:
                self.connretries = int(self.connretries)
            except ValueError:
                logging.error('$PLUGINCONNRETRIES ValueError: {retry} using default.'.format(retry=self.connretries))
                self.connretries = CONNRETRIES
        self.conntimeout = os.getenv('PLUGINCONNTIMEOUT', CONNTIMEOUT)
        if isinstance(self.conntimeout, str):
            try:
                self.conntimeout = int(self.conntimeout)
            except ValueError:
                logging.error('$PLUGINCONNTIMEOUT ValueError: {timeout} using default.'.format(timeout=self.conntimeout))
                self.connretries = CONNTIMEOUT
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS)
        self.tarproc = None
        self.conn = None
        self.tout = None
        self.terr = None
        if isinstance(self.port, str):
            try:
                self.port = int(self.port)
            except ValueError:
                logging.error('$PLUGINPORT ValueError: {port} using default.'.format(port=self.port))
                self.port = DEFAULTPORT
        if isinstance(self.connretries, str):
            try:
                self.connretries = int(self.connretries)
            except ValueError:
                logging.error('$PLUGINCONNRETRIES ValueError: {retry} using default.'.format(port=self.connretries))
                self.port = CONNRETRIES

        logging.info('BaculaConnection: mode={mode} host={host} port={port} token={token}'.format(
            mode=self.mode, host=self.host, port=self.port, token=self.token
        ))

    def authenticate(self):
        logging.debug('Authenticate')
        logging.debug(self.conn)
        if self.conn is not None:
            # first prepare the hello string
            self.conn.send(bytes('{strlen:03}:Hello:{job}'.format(job=self.jobname, strlen=len(self.jobname) + 7), 'ascii'))
            self.conn.send(bytes('Token: {token:64}\n'.format(token=self.token), 'ascii'))
            stat = self.conn.recv(2)
            if stat.decode() == 'OK':
                logging.info('Authenticated.')
            else:
                logging.error('Authentication failure.')
                self.disconnect()
                sys.exit(4)

    def connect(self):
        if self.conn is None:
            sock = socket.socket(socket.AF_INET)
            self.conn = self.context.wrap_socket(sock, server_hostname=self.host)
            for attempt in range(self.connretries):
                try:
                    logging.info('Try to connect to: {host}:{port} {att}/{retry}'.format(host=self.host, port=self.port, att=attempt, retry=self.connretries))
                    self.conn.connect((self.host, self.port))
                except ssl.SSLError as e:
                    logging.debug(e)
                    logging.error('SSLError - cannot continue.')
                    sys.exit(2)
                except OSError as e:
                    logging.debug(e)
                    time.sleep(1)
                    logging.debug('Connected OSError')
                except Exception as e:
                    logging.debug(e)
                    time.sleep(1)
                    logging.debug('Connected Exception')
                    break
                else:
                    logging.info('Connected.')
                    break
            else:
                logging.error('Cannot connect to {host}:{port} max retries exceed.'.format(
                    host=self.host, port=self.port
                ))
                sys.exit(1)
            self.authenticate()

    def disconnect(self):
        if self.conn is not None:
            self.conn.close()
            logging.info('Disconnected.')
            self.conn = None

    def sendfile(self, filename):
        with open(filename, 'rb') as fd:
            while True:
                data = fd.read(65536)
                if len(data) > 0:
                    self.conn.send(data)
                else:
                    break

    def receivefile(self, filename):
        with open(filename, 'wb') as fd:
            while True:
                data = self.conn.recv(65536)
                logging.info('recv:D' + str(len(data)))
                if len(data) > 0:
                    fd.write(data)
                else:
                    break

    def prepare_execute(self):
        if not os.path.exists(TARPIPE):
            os.mkfifo(TARPIPE)
        self.tout = open(TARSTDOUT, 'w')
        self.terr = open(TARSTDERR, 'w')

    def logging_from_file(self, filename):
        with open(filename, 'r') as fd:
            while True:
                log = fd.readline().rstrip()
                if not log:
                    break
                logging.info(log)

    def final_execute(self):
        # close current execution descriptors
        self.tout.close()
        self.terr.close()
        self.disconnect()
        # wait for tar to terminate
        exitcode = -1
        try:
            exitcode = self.tarproc.wait(timeout=self.conntimeout)
        except subprocess.TimeoutExpired:
            logging.error('Timeout waiting {} for tar to proceed. Terminate!'.format(self.conntimeout))
            self.tarproc.terminate()
        # send execution logs
        logging.debug('Final_execute')
        self.connect()
        logging.info('tar exit status:{}'.format(exitcode))
        self.conn.send("{}\n".format(exitcode).encode())
        self.conn.send(b'---- stderr ----\n')
        self.sendfile(TARSTDERR)
        self.conn.send(b'---- list ----\n')
        self.sendfile(TARSTDOUT)
        self.conn.send(b'---- end ----\n')
        self.disconnect()
        logging.info('-- stderr --')
        self.logging_from_file(TARSTDERR)
        logging.info('-- list --')
        self.logging_from_file(TARSTDOUT)
        logging.info('Finish.')

    def err_no_dir_found(self):
        logging.error(ERR_NO_DIR_FOUND.format(dir=self.mode))
        self.terr.write('404 ' + ERR_NO_DIR_FOUND.format(dir=self.mode) + '\n')
        self.final_execute()

    def execute_backup(self):
        logging.debug('Execute_backup')
        self.prepare_execute()
        logging.debug('Executed_prepare_execute')
        if os.path.isdir('/backup'):
            logging.debug('Isdir')
            self.tarproc = subprocess.Popen([TARCMD, '-cvvf', TARPIPE, '-C', '/backup', '.'],
                                            stderr=self.terr,
                                            stdout=self.tout)
            self.sendfile(TARPIPE)
            self.final_execute()
        else:
            logging.debug('Nodir')
            self.err_no_dir_found()

    def execute_restore(self):
        logging.debug('Execute_restore')
        self.prepare_execute()
        if os.path.isdir('/restore'):
            self.tarproc = subprocess.Popen([TARCMD, '-xvvf', TARPIPE, '-C', '/restore'],
                                            stderr=self.terr,
                                            stdout=self.tout)
            self.receivefile(TARPIPE)
            self.final_execute()
        else:
            self.err_no_dir_found()

    def execute(self):
        method = getattr(self, 'execute_' + self.mode, None)
        if method is None:
            logging.error('Invalid mode={mode} of execution!'.format(mode=self.mode))
            self.disconnect()
            sys.exit(3)
        logging.info('BaculaJob: {}'.format(self.jobname))
        self.connect()
        method()


def main():
    logging.basicConfig(format='%(asctime)s:%(levelname)s:%(message)s', level=logging.DEBUG)
    logging.info('Bacula Kubernetes backup helper version {ver}. Copyright {cc}'.format(
        ver=VERSION,
        cc=COPYRIGHT
    ))
    bacula = BaculaConnection()
    bacula.execute()


if __name__ == '__main__':
    main()
