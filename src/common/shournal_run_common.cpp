#include "shournal_run_common.h"

#include "app.h"
#include "conversions.h"
#include "qoutstream.h"
#include "translation.h"

void shournal_run_common::print_summary(uint64_t n_wEvents, uint64_t n_rEvents,
                   uint64_t n_lostEvents,
                   uint64_t n_storedEvents,
                   uint64_t targetFileSize){
    QErr() << qtr("=== %1 summary ===\n"
                  "number of write-events: %2\n"
                  "number of read-events: %3\n"
                  "number of lost events: %4\n"
                  "number of stored read files: %5\n"
                  "size of tmp-file: %6\n")
              .arg(app::CURRENT_NAME)
              .arg(n_wEvents)
              .arg(n_rEvents)
              .arg(n_lostEvents)
              .arg(n_storedEvents)
              .arg(Conversions().bytesToHuman(targetFileSize));
}
