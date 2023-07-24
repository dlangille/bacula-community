# -*- coding: UTF-8 -*-
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
import sys

from baculak8s.io.log import Log
from baculak8s.io.packet_definitions import *
from baculak8s.plugins.plugin import *

CONNECTION_ERROR_TEMPLATE = "Error connecting to the chosen Data Source. {error}."
HOST_NOT_FOUND_CONNECTION_ERROR = "404 Not found. Name or service not known"
HOST_TIMEOUT_ERROR = "Host connection timeout. Maximum retries exceeded"
AUTH_FAILED_CONNECTION_ERROR = "Authentication Failed"
UNEXPECTED_CONNECTION_ERROR = "Unrecognized connection error"
SSL_ERROR = "SSL verification failed"
CONNECTION_REFUSED_TEMPLATE = "Max retry exceeded or Connection refused"


class DefaultIO(object):
    """
        Default Class for performing IO.
        It contains helper methods do read and send packets.
    """

    def send_connection_error(self, error_code, strerror=None):
        if strerror is None:
            message = self.__get_connection_error_message(error_code)
        else:
            message = strerror
        self.send_error(message)

    def send_connection_abort(self, error_code, strerror=None):
        if strerror is None:
            message = self.__get_connection_error_message(error_code)
        else:
            message = strerror
        self.send_abort(message)

    def __get_connection_error_message(self, error_code):
        if error_code == ERROR_HOST_NOT_FOUND:
            return CONNECTION_ERROR_TEMPLATE.format(error=HOST_NOT_FOUND_CONNECTION_ERROR)
        elif error_code == ERROR_HOST_TIMEOUT:
            return CONNECTION_ERROR_TEMPLATE.format(error=HOST_TIMEOUT_ERROR)
        elif error_code == ERROR_AUTH_FAILED:
            return CONNECTION_ERROR_TEMPLATE.format(error=AUTH_FAILED_CONNECTION_ERROR)
        elif error_code == ERROR_SSL_FAILED:
            return CONNECTION_ERROR_TEMPLATE.format(error=SSL_ERROR)
        elif error_code == ERROR_CONNECTION_REFUSED:
            return CONNECTION_ERROR_TEMPLATE.format(error=CONNECTION_REFUSED_TEMPLATE)
        else:
            return CONNECTION_ERROR_TEMPLATE.format(error=UNEXPECTED_CONNECTION_ERROR)

    def send_eod(self):
        packet_header = EOD_PACKET + b"\n"
        sys.stdout.buffer.write(packet_header)
        sys.stdout.flush()
        Log.save_sent_eod(packet_header.decode())

    def send_abort(self, message):
        self.send_packet(STATUS_ABORT, message)

    def send_error(self, message):
        self.send_packet(STATUS_ERROR, message)

    def send_non_fatal_error(self, message):
        self.send_packet(STATUS_NON_FATAL_ERROR, message)
        
    def send_warning(self, message):
        self.send_packet(STATUS_WARNING, message)

    def send_info(self, message):
        self.send_packet(STATUS_INFO, message)

    def send_command(self, message):
        self.send_packet(STATUS_COMMAND, message)

    def send_data(self, data):
        self.send_packet(STATUS_DATA, data, raw=True)

    def send_packet(self, status, packet_content, raw=False):
        """
            Prints a packet to stdout. A packet has the format

            $status$ + $packet_length$ + \n
            $packet_content$ + \n

            where $status$ represents the type of packet sent
            and $packet_length$ has 6 decimal chars.

            If $raw$ is True, $packet_content$ will be handled
            as a Byte String
        """

        bytes_content = packet_content

        if not raw:
            packet_content += "\n"
            bytes_content = packet_content.encode()

        packet_length = str(len(bytes_content)).zfill(6)
        packet_header = "{}{}\n".format(status, packet_length)

        sys.stdout.buffer.write(packet_header.encode())
        sys.stdout.buffer.write(bytes_content)
        sys.stdout.flush()

        if not raw:
            Log.save_sent_packet(packet_header, packet_content)
        else:
            Log.save_sent_data(packet_header)

    def send_file_info(self, info):
        """
           Prints four packages into stdout:
            1 - The files FNAME packet
            2 - The files STAT packet
            3 - The files TSTAMP packet
            4 - A EOD packet
        """

        self.send_command("FNAME:{}".format(info.name))
        self.send_command("TSTAMP:{} {} {}".format(info.accessed_at, info.modified_at, info.created_at))
        self.send_command("STAT:{} {} {} {} {} {}".format(info.type, info.size, info.uid, info.gid, info.mode, info.nlink))
        self.send_eod()

    def send_plugin_object(self, podata):
        """
            Prints PluginObject class information into stdout

        Args:
            podata (class PluginObject): Entity representing information about a Plugin Object
        """
        self.send_command("PLUGINOBJ:{}".format(podata.path))
        self.send_command("PLUGINOBJ_NAME:{}".format(podata.name))
        if podata.cat is not None:
            self.send_command("PLUGINOBJ_CAT:{}".format(podata.cat))
        if podata.type is not None:
            self.send_command("PLUGINOBJ_TYPE:{}".format(podata.type))
        if podata.src is not None:
            self.send_command("PLUGINOBJ_SRC:{}".format(podata.src))
        if podata.uuid is not None:
            self.send_command("PLUGINOBJ_UUID:{}".format(podata.uuid))
        if podata.size is not None:
            self.send_command("PLUGINOBJ_SIZE:{}".format(podata.size))
        if podata.count is not None:
            self.send_command("PLUGINOBJ_COUNT:{}".format(podata.count))
        self.send_eod()

    def send_query_response(self, response):
        key = response[0]
        value = response[1]
        self.send_command("{}={}".format(key, value))

    def read_line(self):
        """
            Reads a line from the stdin buffer

            :return: The line read, as a Byte String, without the newline char
        """
        return sys.stdin.buffer.readline().strip()

    def read_packet(self):
        """
            Reads a packet from the stdin buffer

            :return: 1- The packet header, as a Byte String, without the newline char
                     2- The packet content, as a String, without the newline char
        """
        packet_header = sys.stdin.buffer.readline().strip()
        packet_content = sys.stdin.buffer.readline().strip().decode()
        Log.save_received_packet(packet_header, packet_content)
        return packet_header, packet_content

    def read_eod(self):
        packet_header = self.read_line()
        if not packet_header or packet_header != EOD_PACKET:
            raise ValueError("EOD packet not found")
        Log.save_received_eod(packet_header)
