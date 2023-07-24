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
import json

from baculaswift.io.jobs.backup_io import BACKUP_START_PACKET
from baculaswift.io.jobs.restore_io import RESTORE_START_PACKET, RESTORE_END_PACKET, TRANSFER_START_PACKET
from baculaswift.io.packet_definitions import TERMINATION_PACKET, XATTR_DATA_START, ACL_DATA_START, \
    ESTIMATION_START_PACKET
from baculaswift.io.services.job_end_io import END_JOB_START_PACKET
from baculaswift.io.services.job_info_io import JOB_START_PACKET
from baculaswift.io.services.plugin_params_io import PLUGIN_PARAMETERS_START
from baculaswift.plugins.plugin import DEFAULT_FILE_MODE, DEFAULT_DIR_MODE
from baculaswift.services.job_info_service import TYPE_BACKUP, TYPE_ESTIMATION, TYPE_RESTORE
from tests import BACKEND_PLUGIN_USER, BACKEND_PLUGIN_PWD, BACKEND_PLUGIN_URL, BACKEND_PLUGIN_TYPE
from tests.util.packet_test_util import PacketTestUtil


class HandshakePacketBuilder(PacketTestUtil):
    def build(self, packet_content="Hello {} 1".format(BACKEND_PLUGIN_TYPE)):
        return self.command_packet(packet_content)


class JobInfoStartPacketBuilder(PacketTestUtil):
    def build_invalid(self):
        return self.build("InvalidStartPacket")

    def build(self, content=JOB_START_PACKET):
        packet = self.command_packet(content)
        return packet


class JobInfoBlockBuilder(PacketTestUtil):
    def with_invalid_job_type(self):
        return self.build("InvalidJobType")

    def build(self, job_type, where=None, since=None, replace=None):
        packet = JobInfoStartPacketBuilder().build()
        packet += self.command_packet("Name=az_14125_bcjn")
        packet += self.command_packet("JobID=334")
        packet += self.command_packet("Type={}".format(job_type.upper()))

        if where is not None:
            packet += self.command_packet("Where={}".format(where))

        if since is not None:
            packet += self.command_packet("Since={}".format(since))

        if replace is not None:
            packet += self.command_packet("Replace={}".format(replace))

        packet += self.eod_packet()
        return packet

    def without_job_name(self):
        packet = JobInfoStartPacketBuilder().build()
        packet += self.command_packet("JobID=334")
        packet += self.command_packet("Type=E")
        packet += self.eod_packet()
        return packet

    def without_job_id(self):
        packet = JobInfoStartPacketBuilder().build()
        packet += self.command_packet("Name=az_14125_bcjn")
        packet += self.command_packet("Type=E")
        packet += self.eod_packet()
        return packet

    def without_job_type(self):
        packet = JobInfoStartPacketBuilder().build()
        packet += self.command_packet("Name=az_14125_bcjn")
        packet += self.command_packet("JobID=334")
        packet += self.eod_packet()
        return packet


class PluginParamsStartPacketBuilder(PacketTestUtil):
    def build_invalid(self):
        return self.build(content="InvalidStartPacket")

    def build(self, content=PLUGIN_PARAMETERS_START):
        packet = self.command_packet(content)
        return packet


class PluginParamsBlockBuilder(PacketTestUtil):
    def build(self,
              includes=None,
              regex_includes=None,
              excludes=None,
              regex_excludes=None,
              segment_size=None,
              restore_local_path=None,
              password=True,
              passfile=None):
        packet = PluginParamsStartPacketBuilder().build()
        packet += self.command_packet("User={}".format(BACKEND_PLUGIN_USER))
        packet += self.command_packet("URL={}".format(BACKEND_PLUGIN_URL))

        if password:
            packet += self.command_packet("Password={}".format(BACKEND_PLUGIN_PWD))

        if includes is not None:
            for include in includes:
                packet += self.command_packet("include={}".format(include))

        if regex_includes is not None:
            for regex_include in regex_includes:
                packet += self.command_packet("regex_include={}".format(regex_include))

        if excludes is not None:
            for exclude in excludes:
                packet += self.command_packet("exclude={}".format(exclude))

        if regex_excludes is not None:
            for regex_exclude in regex_excludes:
                packet += self.command_packet("regex_exclude={}".format(regex_exclude))

        if segment_size is not None:
            packet += self.command_packet("be_object_segment_size={}".format(segment_size))

        if restore_local_path is not None:
            packet += self.command_packet("restore_local_path={}".format(restore_local_path))

        if passfile is not None:
            packet += self.command_packet("passfile={}".format(passfile))

        packet += self.command_packet("debug=1")
        packet += self.eod_packet()
        return packet

    def without_url(self):
        packet = PluginParamsStartPacketBuilder().build()
        packet += self.command_packet("User={}".format(BACKEND_PLUGIN_USER))
        packet += self.command_packet("Password={}".format(BACKEND_PLUGIN_PWD))
        packet += self.eod_packet()
        return packet

    def without_user(self):
        packet = PluginParamsStartPacketBuilder().build()
        packet += self.command_packet("Password={}".format(BACKEND_PLUGIN_PWD))
        packet += self.command_packet("URL={}".format(BACKEND_PLUGIN_URL))
        packet += self.eod_packet()
        return packet

    def without_pwd(self):
        packet = PluginParamsStartPacketBuilder().build()
        packet += self.command_packet("User={}".format(BACKEND_PLUGIN_USER))
        packet += self.command_packet("URL={}".format(BACKEND_PLUGIN_URL))
        packet += self.eod_packet()
        return packet


class JobEndPacketBuilder(PacketTestUtil):
    def build(self):
        packet = self.command_packet(END_JOB_START_PACKET)
        packet += TERMINATION_PACKET
        return packet

    def build_invalid(self):
        packet = self.invalid_command_packet()
        packet += TERMINATION_PACKET
        return packet


class BackupCommandBuilder(PacketTestUtil):
    def build(self):
        packet = self.command_packet(BACKUP_START_PACKET)
        return packet


class FilesEstimationCommandBuilder(PacketTestUtil):
    def build(self):
        packet = self.command_packet(ESTIMATION_START_PACKET)
        return packet


class RestoreFileCommandBuilder(PacketTestUtil):
    def with_invalid_file_transfer_start(self, buckets):
        return self.build(buckets, invalid_file_transfer_start=True)

    def with_invalid_xattrs_transfer_start(self, buckets):
        return self.build(buckets, invalid_xattrs_transfer_start=True)

    def with_invalid_acl_transfer_start(self, buckets):
        return self.build(buckets, invalid_acl_transfer_start=True)

    def build(self, buckets,
              with_start_packet=True,
              invalid_file_transfer_start=False,
              invalid_xattrs_transfer_start=False,
              invalid_acl_transfer_start=False,
              fname_without_fsource=False,
              where=None,
              ):

        packet = b''

        if with_start_packet:
            packet += self.command_packet(RESTORE_START_PACKET)

        for bucket in buckets:
            packet += self.__create_files_packets(bucket, invalid_file_transfer_start,
                                                  invalid_xattrs_transfer_start,
                                                  where=where,
                                                  fname_without_fsource=fname_without_fsource)

            packet += self.__create_bucket_packets(bucket, invalid_acl_transfer_start)

        packet += self.command_packet(RESTORE_END_PACKET)
        return packet

    def __create_files_packets(self, bucket, invalid_file_transfer_start=False, invalid_xattrs_transfer_start=False, where=False, fname_without_fsource=False):
        packet = b''
        for file in bucket['files']:
            packet += self.file_info(bucket['name'], file, where=where, fname_without_fsource=fname_without_fsource)
            if "data_packets" not in file:
                file["data_packets"] = True

            if file["data_packets"]:

                if file['size'] > 0:

                    if invalid_file_transfer_start:
                        packet += self.invalid_command_packet()
                    else:
                        packet += self.command_packet(TRANSFER_START_PACKET)

                    if 'content' in file:
                        packet += self.file_contents(file['content'])

                packet += self.file_acl(False)
                packet += self.file_xattrs(invalid_xattrs_transfer_start)

        return packet

    def file_info(self, bucket, file, where=None, fname_without_fsource=False):
        if fname_without_fsource:
            file_source = ""
        else:
            file_source = "@{}".format(BACKEND_PLUGIN_TYPE)

        if where:
            packet = self.command_packet('FNAME:{}/{}/{}/{}'.format(where, file_source, bucket, file['name']))
        else:
            packet = self.command_packet('FNAME:{}/{}/{}'.format(file_source, bucket, file['name']))
        packet += self.command_packet('STAT:F {} 0 0 {} 1 473'.format(file['size'], DEFAULT_FILE_MODE))

        if "modified-at" not in file:
            file["modified-at"] = 2222222222

        packet += self.command_packet('TSTAMP:1111111111 {} 3333333333'.format(file["modified-at"]))
        packet += self.eod_packet()
        return packet

    def file_contents(self, contents):
        packet = b''
        for chunk in contents:
            packet += self.data_packet(chunk)
        packet += self.eod_packet()
        return packet

    def file_xattrs(self, invalid_xattrs_transfer_start):
        if invalid_xattrs_transfer_start:
            packet = self.invalid_command_packet()
        else:
            packet = self.command_packet(XATTR_DATA_START)

        x_attrs = {
            'content-type': "app/pdf",
            'content-encoding': "ascii",
            'content-disposition': "download",
            'x-delete-at': "1999919048",
            'x-object-meta-custom1': "custom_meta1",
            'x-object-meta-custom2': "custom_meta2",
        }
        x_attrs_bytes = json.dumps(x_attrs).encode()

        packet += self.data_packet(x_attrs_bytes)
        packet += self.eod_packet()
        return packet

    def file_acl(self, invalid_acl_transfer_start):
        # Swift does not have File Acl
        if BACKEND_PLUGIN_TYPE == "swift":
            return b''

        if invalid_acl_transfer_start:
            packet = self.invalid_command_packet()
        else:
            packet = self.command_packet(ACL_DATA_START)

        acl = {
            'read': "user1, user2",
            'write': "user3"
        }
        acl_bytes = json.dumps(acl).encode()

        packet += self.data_packet(acl_bytes)
        packet += self.eod_packet()
        return packet

    def __create_bucket_packets(self, bucket, invalid_acl_transfer_start=False):
        packet = self.bucket_info(bucket)

        if "data_packets" not in bucket:
            bucket["data_packets"] = True

        if bucket["data_packets"]:
            packet += self.bucket_acl(invalid_acl_transfer_start)
            packet += self.bucket_xattrs()

        return packet

    def bucket_info(self, bucket):
        packet = self.command_packet('FNAME:@{}/{}/'.format(BACKEND_PLUGIN_TYPE, bucket['name']))

        if "modified-at" not in bucket:
            bucket["modified-at"] = 2222222222

        packet += self.command_packet('STAT:D 12345 0 0 {} 1 473'.format(DEFAULT_DIR_MODE))
        packet += self.command_packet('TSTAMP:1111111111 {} 3333333333'.format(bucket["modified-at"]))
        packet += self.eod_packet()
        return packet

    def bucket_xattrs(self):
        packet = self.command_packet(XATTR_DATA_START)
        xattrs = {
            'x-container-meta-quota-bytes': "1000000",
            'x-container-meta-quota-count': "100",
            'x-container-meta-web-directory-type': "text/directory",
            'x-container-meta-custom1': "custom_meta1",
            'x-container-meta-custom2': "custom_meta2",
        }
        x_attrs_bytes = json.dumps(xattrs).encode()

        packet += self.data_packet(x_attrs_bytes)
        packet += self.eod_packet()
        return packet

    def bucket_acl(self, invalid_acl_transfer_start):
        if invalid_acl_transfer_start:
            packet = self.invalid_command_packet()
        else:
            packet = self.command_packet(ACL_DATA_START)

        acl = {
            'read': "user1, user2",
            'write': "user3"
        }
        acl_bytes = json.dumps(acl).encode()

        packet += self.data_packet(acl_bytes)
        packet += self.eod_packet()
        return packet


class BackendCommandBuilder(PacketTestUtil):
    def build(self, job_type, buckets=None, where=None, includes=None, segment_size=None, restore_local_path=None):
        packet = HandshakePacketBuilder().build()
        packet += JobInfoBlockBuilder().build(job_type, where=where)
        packet += PluginParamsBlockBuilder().build(includes=includes, segment_size=segment_size, restore_local_path=restore_local_path)

        if job_type == TYPE_BACKUP:
            packet += BackupCommandBuilder().build()

        elif job_type == TYPE_ESTIMATION:
            packet += FilesEstimationCommandBuilder().build()

        elif job_type == TYPE_RESTORE:
            packet += RestoreFileCommandBuilder().build(buckets)

        else:
            raise ValueError("Invalid job_type!")

        packet += JobEndPacketBuilder().build()
        return packet
