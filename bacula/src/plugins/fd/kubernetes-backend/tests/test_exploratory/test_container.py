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


import unittest
from time import sleep

from swiftclient.service import SwiftUploadObject

from tests.base_tests import BaseTest


class ContainerTest(BaseTest):
    def test_should_create_container(self):
        container_name = "cats"
        self.swift_connection.put_container(container_name)
        containers = self.swift_connection.get_account()[1]
        self.assertEqual(len(containers), 1)

    def test_should_list_containers(self):
        self.__create_containers(5)
        response = self.swift_service.list()

        for page in response:
            print(len(page["listing"]))
            print(page["listing"])

    def __create_containers(self, amount):

        for i in range(0, amount):
            self.swift_connection.put_container("container={}".format(i))

    def test_should_list_container_metadata(self):
        container_name = "cats"
        self.swift_connection.put_container(container_name)
        stats = self.swift_service.stat(container=container_name)
        print(stats['headers'])
        sleep(4)
        self.swift_connection.put_object(container_name, "cat1.txt", "")
        stats = self.swift_service.stat(container=container_name)
        print(stats['headers'])

    def test_should_list_container_objects(self):
        container_name = "WORLD"
        files = ["AMERICA/NORTH/EUA/eua.txt",
                 "AMERICA/NORTH/CANADA/canada.txt",
                 "AMERICA/SOUTH/BRAZIL/brazil.txt"]

        fc1 = FileLikeObject([self.gen_random_bytes(65553) for i in range(1)])
        obj1 = SwiftUploadObject(fc1, files[0])

        fc2 = FileLikeObject([self.gen_random_bytes(65553) for i in range(1)])
        obj2 = SwiftUploadObject(fc2, files[1])

        fc3 = FileLikeObject([self.gen_random_bytes(65553) for i in range(1)])
        obj3 = SwiftUploadObject(fc2, files[2])

        r_gen = self.swift_service.upload(container_name, [obj1, obj2, obj3])

        for r in r_gen:
            pass

        self.swift_connection.put_container(container_name)
        list_gen = self.swift_connection.get_container(container=container_name)
        print(list_gen)

class FileLikeObject(object):
    def __init__(self, file_chunks):
        self.file_chunks = file_chunks

    def read(self, size=-1):
        if not self.file_chunks:
            return None

        return self.file_chunks.pop()

    def next_chunk(self, chunk_size):
        pass




if __name__ == '__main__':
    unittest.main()
