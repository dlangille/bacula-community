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
import time

K8S_NUM_MAX_TRIES_ERROR = "Reached maximum number of tries! Message: {message}"
NUM_MAX_TRIES = 20

def apply(condition, iterable):
    # Helper method to map lambdas on iterables
    # Created just for readability
    return list(map(condition, iterable))


def wait_until_resource_is_ready(action, error_msg = 'Reached num maximum tries.', sleep = 3):
    logging.debug('Waiting until resource is ready')
    tries = 0
    is_ready = False
    while not is_ready:
        is_ready = action()
        if tries >= NUM_MAX_TRIES:
            logging.debug("Reached num maximum tries.")
            return { 'error': K8S_NUM_MAX_TRIES_ERROR.format(message=error_msg)}
        time.sleep(sleep)
    return {}