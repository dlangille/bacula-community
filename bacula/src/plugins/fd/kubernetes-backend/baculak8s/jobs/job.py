# Bacula(R) - The Network Backup Solution
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

import logging
import sys
import time
from abc import ABCMeta, abstractmethod

from baculak8s.io.log import Log

INVALID_START_TEMPLATE = "Invalid start packet. Expected packet: {}"
KUBERNETES_CODE_INFO = "Connected to Kubernetes {major}.{minor} - {git_version}."


class Job(metaclass=ABCMeta):
    """
        Abstract Base Class for all the Backend Jobs
    """

    def __init__(self, plugin, io, params):
        self._plugin = plugin
        self._io = io
        self._params = params

    @abstractmethod
    def execute(self):
        """
            Executes the Job
        """
        raise NotImplementedError

    @abstractmethod
    def execution_loop(self):
        """
            Execution loop subroutine
        """
        raise NotImplementedError

    def _start(self, expected_start_packet):
        self._read_start(expected_start_packet, onError=self._abort)
        if self._params.get('level', 'F') != 'F':
            logging.error("Unsupported backup level. Doing FULL backup.")
            self._io.send_warning("Unsupported backup level. Doing FULL backup.")
        self._connect()
        self._io.send_eod()

    def _read_start(self, start_packet, onError):
        _, packet = self._io.read_packet()
        if packet != start_packet:
            self._io.send_abort(INVALID_START_TEMPLATE.format(start_packet))
            onError()

    def _connect(self):
        response = self._plugin.connect()

        if 'error' in response:
            logging.debug("response data:{}".format(response))
            if 'error_code' in response:
                self._io.send_connection_error(response['error_code'])
            else:
                self._io.send_connection_error(0, strerror=response['error'])
            # Reads termination packet
            packet_header = self._io.read_line()
            Log.save_received_termination(packet_header)
            sys.exit(0)
        else:
            if self._params.get("type", None) == 'b':
                # display some info to user
                data = response.get('response')
                if data is not None:
                    self._io.send_info(KUBERNETES_CODE_INFO.format(
                        major=data.major,
                        minor=data.minor,
                        git_version=data.git_version,
                    ))

    def _handle_error(self, error_message):
        if self._params.get("abort_on_error", None) == "1":
            self._io.send_abort(error_message)
            self._abort()
        else:
            self._io.send_error(error_message)

    def _abort(self):
        self._plugin.disconnect()
        sys.exit(0)
