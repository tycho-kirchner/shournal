
#include <QDebug>
#include <dlfcn.h>

#include "logger.h"


#include "attached_bash.h"
#include "os.h"

static int read_seq(){
    const char* _LIBSHOURNAL_SEQ_COUNTER = "_LIBSHOURNAL_SEQ_COUNTER";
    const char* seq_val = getenv(_LIBSHOURNAL_SEQ_COUNTER);
    if(seq_val == nullptr){
        logWarning << qtr("Required environment variable '%1' "
                          "is unset.").arg(_LIBSHOURNAL_SEQ_COUNTER);
        return -1;
    }
    int seq;
    try {
        qVariantTo_throw(seq_val, &seq);
    } catch (const ExcQVariantConvert& ex) {
        logWarning << "Failed to convert sequnce:"
                   << ex.descrip();
        return -1;
    }
    return seq;
}

/// @throws ExcOs
AttachedBash::AttachedBash() :
    m_lastSeq(1)
{}

void AttachedBash::handleEnable()
{
    m_lastSeq = read_seq();
}

/// The command is considered valid, if the command-counter
/// has changed since the last call of this function or handleEnable().
/// This function is meant to be called only *once*
/// per command sequence.
bool AttachedBash::cmdCounterJustIncremented()
{
    int current_seq = read_seq();
    if(current_seq == -1){
        return false; // error
    }
    if(current_seq == m_lastSeq){
        return false;
    }
    m_lastSeq = current_seq;
    return true;
}
