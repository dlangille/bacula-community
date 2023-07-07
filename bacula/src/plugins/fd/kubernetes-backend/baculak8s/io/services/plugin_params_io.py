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

from baculak8s.io.default_io import DefaultIO
from baculak8s.io.log import Log
from baculak8s.io.packet_definitions import EOD_PACKET

PLUGIN_PARAMETERS_START = "Params"
INVALID_PLUGIN_PARAMETERS_START = "Invalid Plugin Parameters Start Packet"
INVALID_PLUGIN_PARAMETERS_BLOCK = "Invalid Plugin Parameters Block"
URL_NOT_FOUND = "Parameter URL not found on Plugin Parameters"
USER_NOT_FOUND = "Parameter User not found on Plugin Parameters"
PWD_NOT_FOUND = "Parameter Password not found on Plugin Parameters"
PASSFILE_NOT_FOUND = "Passfile not found. ERR=No such file or directory"
PWD_INSIDE_PASSFILE_NOT_FOUND = "Password inside passfile not found"
RESTORE_LOCAL_WITHOUT_WHERE = "Restore Local plugin parameter without Where Plugin Parameter"


class PluginParamsIO(DefaultIO):

    def read_plugin_params(self):
        """
                Reads blocks of parameters:

                    "C000111\n"     (Command packet header)
                    key1=value1\n   (Parameter)
                    "C000222\n"
                    key2=value2\n
                    "C000333\n"
                    key3=value3\n
                    ...

                until an EOD packet (F000000) is found

                :return: A dictionary containing the parameters

        """

        block = {
            "includes": [],
            "regex_includes": [],
            "excludes": [],
            "regex_excludes": [],
            "namespace": [],
            "persistentvolume": [],
            "storageclass": [],
        }
        while True:
            packet_header = self.read_line()
            if not packet_header:
                raise ValueError("Packet Header not found")

            if packet_header == EOD_PACKET:
                Log.save_received_eod(packet_header)
                return block
            else:
                packet_content = self.read_line().decode()
                param = packet_content.split("=", 1)

                # converts key to lowercase
                param[0] = param[0].lower()

                # handle single word param without equal sign
                if len(param) < 2:
                    param.append(True)

                # Allow simple quote to identify string parameters.
                if isinstance(param[1], str):
                    if param[1][0] == "'":
                        param[1] = ''.join(param[1].split("'"))

                # handle array parameters automatically
                if param[0] in block.keys():
                    if isinstance(block[param[0]], list):
                        block[param[0]].append(param[1])
                    else:
                        _b = block[param[0]]
                        block[param[0]] = [_b, param[1]]
                else:
                    block[param[0]] = param[1]

                Log.save_received_packet(packet_header, packet_content)
