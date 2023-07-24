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

from baculak8s.util.lambda_util import apply
from tests.base_tests import BaseTest
from tests.util.os_test_util import gen_random_bytes


class K8STest(BaseTest):
    """
        Class with utility methods for dealing with K8S
    """

    def setUp(self):
        super().setUp()
        self.config_test_k8sclient()

    def tearDown(self):
        super().tearDown()
        self.delete_all_containers()

    def config_test_k8sclient(self):
        pass

    def delete_all_containers(self):
        pass

    def create_test_data(self, container, file_content, filename):
        pass

    def create_container_new(self, buckets):
        pass

    def upload_file(self, container_name, filepath, filename):
        with open(filepath + "/" + filename, 'rb') as local:
            self.swift_connection.put_object(
                container_name,
                filename,
                contents=local,
                content_type='text/plain'
            )

    def upload_files(self, container, files, file_chunk_count=0, file_chunk_size=0):
        pass

    def create_containers(self, amount):
        containers = []

        for i in range(0, amount):
            container_name = "container_%d" % i
            self.swift_connection.put_container(container_name)
            containers.append(container_name)

        return containers

    def put_containers_on_swift(self, containers):
        for container in containers:
            self.swift_connection.put_container(container)

    def upload_test_objects(self, container, path, filenames):
        for filename in filenames:
            self.upload_file(container, path, filename)

    def verify_containers(self, buckets, where=None):

        if where is not None:
            splitted = where.split("/")
            splitted = list(filter(None, splitted))
            header, uploaded_files = self.swift_connection.get_container(splitted[0])
            uploaded_names = apply(lambda f: f['name'], uploaded_files)
            uploaded_bytes = apply(lambda f: f['bytes'], uploaded_files)

            for bucket in buckets:
                expected_files = bucket['files']

                for file in expected_files:
                    expected_name = file["name"]
                    if len(splitted) > 1:
                        path = "/".join(splitted[1:])
                        expected_name = "{}/{}".format(path, expected_name)
                    self.assertIn(expected_name, uploaded_names)
                    self.assertIn(file['size'], uploaded_bytes)


        else:
            for bucket in buckets:
                expected_files = bucket['files']
                header, uploaded_files = self.swift_connection.get_container(bucket['name'])
                uploaded_names = apply(lambda f: f['name'], uploaded_files)
                uploaded_bytes = apply(lambda f: f['bytes'], uploaded_files)

                for file in expected_files:
                    self.assertIn(file['name'], uploaded_names)
                    self.assertIn(file['size'], uploaded_bytes)

                self.assertIn('x-container-meta-custom1', header)


class FileLikeObject(object):
    def __init__(self, file_chunks):
        self.file_chunks = file_chunks

    def read(self, size=-1):
        if not self.file_chunks:
            return None

        return self.file_chunks.pop()

    def next_chunk(self, chunk_size):
        pass
