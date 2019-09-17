

#include "socket_message.h"



const char* socket_message::socketMsgToStr(E_SocketMsg msg){
    switch (msg) {
    case E_SocketMsg::SETUP_DONE: return "SETUP_DONE";
    case E_SocketMsg::SETUP_FAIL: return "SETUP_FAIL";
    case E_SocketMsg::CLEAR_EVENTS: return "CLEAR_EVENTS";
    case E_SocketMsg::COMMAND: return "COMMAND";
    case E_SocketMsg::RETURN_VALUE: return "RETURN_VALUE";
    case E_SocketMsg::EMPTY: return "EMPTY";
    case E_SocketMsg::LOG_MESSAGE: return "LOG_MESSAGE";
    case E_SocketMsg::CMD_START_DATETIME: return "CMD_START_DATETIME";
    case E_SocketMsg::ENUM_END: return "ENUM_END";
    }
    return "UNHANDLED ENUM CASE";
}
