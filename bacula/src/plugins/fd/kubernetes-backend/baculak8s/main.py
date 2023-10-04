#!/usr/bin/env python3

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

from baculak8s.io.log import Log, LogConfig
from baculak8s.jobs.job_factory import JobFactory
from baculak8s.plugins.plugin_factory import PluginFactory
from baculak8s.services.handshake_service import HandshakeService
from baculak8s.services.job_end_service import JobEndService
from baculak8s.services.job_info_service import JobInfoService
from baculak8s.services.plugin_params_service import PluginParamsService
from baculak8s.services.unexpected_error_service import UnexpectedErrorService
from baculak8s.util.dict_util import merge_two_dicts


def main():

    try:
        LogConfig.start()
        plugin_name = HandshakeService().execute()
        job_info = JobInfoService().execute()
        plugin_params = PluginParamsService(job_info).execute()
        LogConfig.handle_params(job_info, plugin_params)
        merged_params = merge_two_dicts(job_info, plugin_params)
        plugin = PluginFactory.create(plugin_name, merged_params)
        job = JobFactory.create(merged_params, plugin)
        job.execute()
        JobEndService(merged_params, plugin).execute()
    except Exception as E:
        Log.save_exception(E)
        UnexpectedErrorService().execute()
        exit_code = 1
        Log.save_exit_code(exit_code)
        return exit_code
    except SystemExit:
        pass

    exit_code = 0
    Log.save_exit_code(exit_code)
    return exit_code


if __name__ == '__main__':
    main()
