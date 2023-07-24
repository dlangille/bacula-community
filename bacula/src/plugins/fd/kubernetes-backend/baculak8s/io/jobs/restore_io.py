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

import re
import sys
from enum import Enum
import logging

from baculak8s.entities.file_info import FileInfo
from baculak8s.io.default_io import DefaultIO
from baculak8s.io.log import Log
from baculak8s.io.packet_definitions import ACL_DATA_START, XATTR_DATA_START, EOD_PACKET, STATUS_DATA
from baculak8s.plugins.k8sbackend.k8sfileinfo import k8sfileobjecttype

RESTORE_START = "RestoreStart"
INVALID_RESTORE_START_PACKET = "Invalid restore job start packet"
RESTORE_END_PACKET = "FINISH"
SUCCESS_PACKET = "OK"
SKIP_PACKET = "SKIP"
FILE_TRANSFER_START = "DATA"
INVALID_XATTRS_TRANSFER_START_PACKET = "Invalid extended attributes transfer start packet. Aborting"
INVALID_ACL_TRANSFER_START_PACKET = "Invalid access control list transfer start packet. Aborting"
RESTORE_LOOP_ERROR = "Invalid packet during restore loop."
FNAME_WITHOUT_FSOURCE_ERROR = "Invalid FNAME packet. It should have information about the Files Source."

XATTR_ERROR_TEMPLATE = "Error while transferring files extended attributes to the chosen Data Source" \
                       "\nFile: {}\nBucket: {}.\n"

ACL_ERROR_TEMPLATE = "Error while transferring files access control list to the chosen Data Source" \
                     "\nFile: {}\nBucket: {}.\n"

FILE_ERROR_TEMPLATE = "Error while transferring file content to the chosen Data Source" \
                      "\n\tFile: {}\n\tNamespace: {}\n\tdetails: {}"

BUCKET_ERROR_TEMPLATE = "Error while creating a Bucket on the chosen Data Source" \
                      "Bucket: {}.\n"

COMMA_SEPARATOR_NOT_SUPPORTED = "Comma separator not supported yet."


class RestorePacket(Enum):
    FILE_INFO = 1
    ACL_START = 2
    XATTR_START = 3
    RESTORE_END = 4
    INVALID_PACKET = 5


class RestoreIO(DefaultIO):
    def next_loop_packet(self, onError):
        _, packet = self.read_packet()
        logging.debug('next_loop_packet:packet:' + str(packet))
        if packet is None:
            return RestorePacket.INVALID_PACKET, None

        if packet == RESTORE_END_PACKET:
            return RestorePacket.RESTORE_END, None

        if packet.startswith("FNAME:"):
            file_info = self.__read_file_info(packet, onError)
            return RestorePacket.FILE_INFO, file_info

        if packet == ACL_DATA_START:
            return RestorePacket.ACL_START, None

        if packet == XATTR_DATA_START:
            return RestorePacket.XATTR_START, None

        return RestorePacket.INVALID_PACKET, None

    def __read_file_info(self, full_fname, onError):
        """
        Reads four packages from stdin:
            1 - The files FNAME packet
            2 - The files STAT packet
            3 - The files TSTAMP packet
            4 - An EOD packet

        :return: The file_info data structure
        """

        full_fname = full_fname.replace("FNAME:", "").rstrip("/")

        if "@" not in full_fname:
            _, full_stat = self.read_packet()
            _, full_tstamp = self.read_packet()
            self.read_eod()
            self.send_abort(FNAME_WITHOUT_FSOURCE_ERROR)
            onError()
            return

        where_param = self.__read_where_parameter(full_fname)

        if where_param is not None:
            full_fname = full_fname.replace(where_param, '', 1).lstrip('/')

        # creates an array like:
        # ['@kubernetes', 'namespaces', '$namespace', '$object', '$file.yaml']
        # ['@kubernetes', 'namespaces', '$namespace', '$object', '$file.tar']
        # ['@kubernetes', 'namespaces', '$namespace', '$file.yaml']
        # ['@kubernetes', 'persistentvolumes', '$file.yaml']
        fname = full_fname.split("/", 4)

        _, full_stat = self.read_packet()

        # creates an array with [$type$, $size$, $uid$, $gid$, $mode$, $nlink$]
        fstat = re.sub("STAT:", '', full_stat).split(' ')

        _, full_tstamp = self.read_packet()

        # creates an array with [$atime$, $mtime$, $ctime$]
        ftstamp = re.sub("TSTAMP:", '', full_tstamp).split(' ')

        self.read_eod()

        # debugging
        logging.debug("fstat:" + str(fstat))
        logging.debug("fname:" + str(fname))
        logging.debug("ftstamp:" + str(ftstamp))

        objtype = k8sfileobjecttype(fname)
        return FileInfo(
            # The name may be empty if we have a bucket file
            name=fname[-1],
            ftype=fstat[0],
            size=int(fstat[1]),
            uid=fstat[2],
            gid=fstat[3],
            mode=fstat[4],
            nlink=fstat[5],
            index=fstat[6],
            accessed_at=int(ftstamp[0]),
            modified_at=int(ftstamp[1]),
            created_at=int(ftstamp[2]),
            namespace=objtype['namespace'],
            objtype=objtype['obj'],
            fullfname=fname
        )

    def __read_where_parameter(self, full_fname):
        if full_fname.startswith("@"):
            where_param = None
        else:
            # we read the where param part of the FNAME packet
            index_fsource = full_fname.index("/@")
            where_param = full_fname[0:index_fsource]
            where_param = where_param.rstrip("/")

        return where_param

    def read_data(self):
        """
            Reads a data packet from the stdio:

                "D000123\n" (Data packet header)
                "chunk"     (Data packet)

            :return: The data packet content, or
                     None if an EOD packet (F000000) is found instead
        """
        header = self.read_line()
        if not header:
            raise ValueError("Packet Header not found")
        logging.debug('io.read_data: {}'.format(header))
        if header == EOD_PACKET:
            Log.save_received_eod(header)
            return None

        # Removes the status of the header to obtain data content length
        chunk_length = int(header.decode().replace(STATUS_DATA, ''))
        chunk = sys.stdin.buffer.read(chunk_length)
        Log.save_received_data(header)
        return chunk


class FileContentReader(RestoreIO):
    """
        Class used to read chunks of file content from standard input.
    """

    def __init__(self):
        self.finished = False

    def read(self, size=None):
        if self.finished:
            return None

        data = self.read_data()
        if not data:
            self.finished = True
        return data

    def finished_transfer(self):
        return self.finished
