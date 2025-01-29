#pragma once

#include <stdint.h>

#include "util.h"
#include "qoptarg.h"

namespace shournal_run_common {

void print_summary(uint64_t n_wEvents, uint64_t n_rEvents,
                   uint64_t n_lostEvents,
                   uint64_t n_storedEvents,
                   uint64_t targetFileSize);

QOptArg mkarg_cfgdir();
QOptArg mkarg_datadir();

} // namespace shournal_run_common


