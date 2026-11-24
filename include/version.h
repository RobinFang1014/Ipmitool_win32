#pragma once

/*
 * Version of the ipmitool native Windows port.
 * This is the only place the version is defined. It is used by
 * config.h (ipmitool -V) and Resource.rc (exe Properties > Details).
 *
 * Format: <upstream ipmitool version>-win32.<port revision>
 *
 * Release tags must be "v" + IPMITOOL_WIN32_VERSION_STR,
 * e.g. v1.8.19-win32.1. .github/workflows/build.yml checks this.
 *
 * When releasing a new port revision, bump IPMITOOL_WIN32_VER_REVISION
 * and IPMITOOL_WIN32_VERSION_STR together.
 */
#define IPMITOOL_WIN32_VER_MAJOR     1
#define IPMITOOL_WIN32_VER_MINOR     8
#define IPMITOOL_WIN32_VER_PATCH     19
#define IPMITOOL_WIN32_VER_REVISION  1

#define IPMITOOL_WIN32_VERSION_STR   "1.8.19-win32.1"
