// SPDX-License-Identifier: BSD-3-Clause
/***************************************************************************

    coinline.cpp

    Per-subtarget constants for SUBTARGET=coinline (CoinLine Millennium).

****************************************************************************/

#include "emu.h"
#include "main.h"

#define APPNAME                 "CoinLine"
#define APPNAME_LOWER           "coinline"
#define CONFIGNAME              "coinline"
#define COPYRIGHT               "CoinLine emulator (MAME-derived)\nGPL-2.0-or-later applies to linked MAME components."
#define COPYRIGHT_INFO          "CoinLine / MAME"

const char *emulator_info::get_appname() { return APPNAME; }
const char *emulator_info::get_appname_lower() { return APPNAME_LOWER; }
const char *emulator_info::get_configname() { return CONFIGNAME; }
const char *emulator_info::get_copyright() { return COPYRIGHT; }
const char *emulator_info::get_copyright_info() { return COPYRIGHT_INFO; }
