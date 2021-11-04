# -*- coding: UTF-8 -*-
# Bacula(R) - The Network Backup Solution
#
#  Copyright (C) 2000-2023 Kern Sibbald
#
#  The original author of Bacula is Kern Sibbald, with contributions
#  from many others, a complete list can be found in the file AUTHORS.
#
#  You may use this file and others of this release according to the
#  license defined in the LICENSE file, which includes the Affero General
#  Public License, v3.0 ("AGPLv3") and some additional permissions and
#  terms pursuant to its AGPLv3 Section 7.
#
#  This notice must be preserved when any source code is
#  conveyed and/or propagated.
#
#  Bacula(R) is a registered trademark of Kern Sibbald.

class PluginObject(object):
    """
        Entity representing information about a Plugin Object
    """

    def __init__(self, path, name, cat=None, potype=None, src=None, uuid=None, count=None, size=None):
        self.path = path
        self.name = name
        self.cat = cat
        self.type = potype
        self.src = src
        self.uuid = uuid
        self.count = count
        self.size = size

    def __str__(self):
        return '{{PluginObject path: {} name:{} cat:{} type:{} src:{} uuid:{} count:{} size:{}}}'\
            .format(self.path, self.name, self.cat, self.type, self.src, self.uuid, self.count, self.size)
