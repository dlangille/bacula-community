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

STATUS_COMMAND = "C"
STATUS_DATA = "D"
STATUS_ABORT = "A"
STATUS_ERROR = "E"
STATUS_NON_FATAL_ERROR = "Q"
STATUS_WARNING = "W"
STATUS_INFO = "I"

EOD_PACKET = b'F000000'
TERMINATION_PACKET = b'T000000'

UNEXPECTED_ERROR_PACKET = "Unexpected error. Please check log for details"

FILE_DATA_START = "DATA"
XATTR_DATA_START = "XATTR"
ACL_DATA_START = "ACL"
ESTIMATION_START_PACKET = "EstimateStart"
QUERY_START_PACKET = "QueryStart"
INVALID_ESTIMATION_START_PACKET = "Invalid estimation job start packet"
OBJECT_PAGE_ERROR = "Error retrieving a page of buckets."
FILE_INFO_ERROR = "Error retrieving information about a file."
