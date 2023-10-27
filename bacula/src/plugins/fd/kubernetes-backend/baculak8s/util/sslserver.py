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

import os
import socket
import ssl
import time

from baculak8s.util.token import *

DEFAULTTIMEOUT = 600
DEFAULTCERTFILE = "/opt/bacula/etc/bacula-backup.cert"
DEFAULTKEYFILE = "/opt/bacula/etc/bacula-backup.key"
CONNECTIONSERVER_AUTHTOK_ERR1 = 'ConnectionServer: Authentication token receiving timeout!'
CONNECTIONSERVER_AUTHTOK_ERR2 = 'ConnectionServer: Authentication token error!'
CONNECTIONSERVER_AUTHTOK_ERR3 = '{ "message": "Cert files does not exist! Cannot prepare Connection Service!" }'
CONNECTIONSERVER_AUTHTOK_ERR4 = '{ "message": "ConnectionServer:Cannot bind to socket! Err={}" }'
CONNECTIONSERVER_AUTHTOK_ERR5 = 'ConnectionServer: Timeout waiting...'
CONNECTIONSERVER_AUTHTOK_ERR6 = 'ConnectionServer: Invalid Hello Data received!'
CONNECTIONSERVER_HELLO_ERR1 = 'ConnectionServer: Hello data receiving timeout!'
CONNECTIONSERVER_HELLO_ERR2 = 'ConnectionServer: Invalid Hello data packet: "{}"'
CONNECTIONSERVER_HELLO_ERR3 = 'ConnectionServer: Invalid Hello header: "{}"'


class ConnectionServer(object):

    def __init__(self, host, port=9104, token=None, certfile=None, keyfile=None, timeout=DEFAULTTIMEOUT,
                 *args, **kwargs):
        super(ConnectionServer, self).__init__(*args, **kwargs)
        self.connstream = None
        self.token = token if token is not None else generate_token()
        self.timeout = timeout
        try:
            self.timeout = int(self.timeout)
        except ValueError:
            self.timeout = DEFAULTTIMEOUT
        self.timeout = max(1, self.timeout)
        socket.setdefaulttimeout(self.timeout)
        self.bindsocket = socket.socket()
        self.sslcontext = ssl.SSLContext(ssl.PROTOCOL_TLS)
        self.host = host
        self.port = port
        self.certfile = certfile if certfile is not None else DEFAULTCERTFILE
        self.keyfile = keyfile if keyfile is not None else DEFAULTKEYFILE

    def streamrecv(self, size):
        try:
            data = self.connstream.recv(size)
        except socket.timeout:
            data = None
        return data

    def streamsend(self, data):
        status = True
        try:
            self.connstream.send(data)
        except socket.timeout:
            status = False
        finally:
            return status

    def authenticate(self):
        hello = self.gethello()
        if not isinstance(hello, dict) or 'error' in hello:
            logging.debug(CONNECTIONSERVER_AUTHTOK_ERR6)
            return {
                'error': CONNECTIONSERVER_AUTHTOK_ERR6,
            }
        response = hello.get('response')
        if response is not None and response[0] != 'Hello':
            logging.debug(CONNECTIONSERVER_AUTHTOK_ERR6)
            return {
                'error': CONNECTIONSERVER_AUTHTOK_ERR6,
            }

        try:
            data = self.connstream.recv(auth_data_length())
        except socket.timeout:
            logging.debug(CONNECTIONSERVER_AUTHTOK_ERR1)
            return {
                'error': CONNECTIONSERVER_AUTHTOK_ERR1,
            }
        if check_auth_data(self.token, data):
            self.connstream.send(b'OK')
            logging.debug('ConnectionServer:Authenticated')
            return {}
        else:
            self.connstream.send(b'NO')
            logging.error(CONNECTIONSERVER_AUTHTOK_ERR2)
            return {
                'error': CONNECTIONSERVER_AUTHTOK_ERR2,
            }

    def gethello(self) -> dict:
        data = ""
        try:
            data = self.connstream.recv(4)
        except socket.timeout:
            logging.debug(CONNECTIONSERVER_HELLO_ERR1)
            return {
                'error': CONNECTIONSERVER_HELLO_ERR1,
            }
        ddata = data.decode()
        if ddata[3] != ':':
            logging.debug(CONNECTIONSERVER_HELLO_ERR3.format(ddata))
            return {
                'error': CONNECTIONSERVER_HELLO_ERR3.format(ddata),
            }
        ddata = ddata[:3]
        try:
            datalen = int(ddata)
        except ValueError:
            logging.debug(CONNECTIONSERVER_HELLO_ERR2.format(ddata))
            return {
                'error': CONNECTIONSERVER_HELLO_ERR2.format(ddata),
            }
        try:
            data = self.connstream.recv(datalen)
        except socket.timeout:
            logging.debug(CONNECTIONSERVER_HELLO_ERR1)
            return {
                'error': CONNECTIONSERVER_HELLO_ERR1,
            }
        ddata = data.decode().split(':')
        logging.debug(ddata)
        return { 'response': ddata }

    def close(self):
        self.connstream.shutdown(socket.SHUT_RDWR)
        self.connstream.close()

    def shutdown(self):
        self.bindsocket.close()

    def listen(self):
        if not os.path.exists(self.certfile) or not os.path.exists(self.keyfile):
            logging.error(CONNECTIONSERVER_AUTHTOK_ERR3)
            return {
                'error': True,
                'descr': CONNECTIONSERVER_AUTHTOK_ERR3,
            }
        self.sslcontext.load_cert_chain(certfile=self.certfile, keyfile=self.keyfile)
        ops = 0
        lastexcept = ""
        for ops in range(self.timeout):
            try:
                self.bindsocket.bind((self.host, self.port))
            except OSError as e:
                logging.error(e)
                lastexcept = str(e)
                time.sleep(5)
            else:
                break
        if ops == self.timeout - 1:
            logging.error(CONNECTIONSERVER_AUTHTOK_ERR4.format(lastexcept))
            return {
                'error': True,
                'descr': CONNECTIONSERVER_AUTHTOK_ERR4.format(lastexcept)
            }
        logging.debug('ConnectionServer:Listening...')
        self.bindsocket.listen(5)
        return {}

    def handle_connection(self, process_client_data):
        try:
            newsocket, fromaddr = self.bindsocket.accept()
        except socket.timeout:
            logging.error(CONNECTIONSERVER_AUTHTOK_ERR5)
            return {
                'error': CONNECTIONSERVER_AUTHTOK_ERR5,
                'should_remove_pod': 1,
            }
        logging.debug("ConnectionServer:Connection from: {}".format(fromaddr))
        self.connstream = self.sslcontext.wrap_socket(newsocket, server_side=True)
        try:
            authresp = self.authenticate()
            if 'error' in authresp:
                return authresp
            process_client_data(self.connstream)
        finally:
            logging.debug('ConnectionServer:Finish - disconnect.')
            try:
                self.connstream.shutdown(socket.SHUT_RDWR)
            except Exception as e:
                logging.debug('throw exception when disconnect.')
                logging.error(e)
            logging.debug('ConnectionServer:Finish - shutdown.')
            self.connstream.close()
            logging.debug('ConnectionServer:Finish - closed.')
        return {}
