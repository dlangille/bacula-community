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


from baculaswift.io.packet_definitions import EOD_PACKET


class PacketTestUtil(object):
    def data_packet(self, data):
        packet = ("D{}\n".format(str(len(data)).zfill(6)).encode())
        packet += data
        return packet

    def invalid_command_packet(self):
        return self.command_packet("invalid_command")

    def command_packet(self, packet_content):
        packet_content += "\n"
        packet_header = "C{}\n".format(str(len(packet_content)).zfill(6))
        return (packet_header + packet_content).encode()

    def eod_packet(self):
        return EOD_PACKET + b"\n"
