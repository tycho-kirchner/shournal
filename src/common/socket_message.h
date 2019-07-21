#pragma once


namespace socket_message {

/// Messages send from shell observation to shournal process or vice versa
enum class E_SocketMsg { SETUP_DONE, SETUP_FAIL, CLEAR_EVENTS,
                         COMMAND, RETURN_VALUE, EMPTY,
                         LOG_MESSAGE, ENUM_END };

const char* socketMsgToStr(E_SocketMsg msg);


}

