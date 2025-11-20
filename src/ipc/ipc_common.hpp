#ifndef IPC_COMMON
#define IPC_COMMON

#include <string>

namespace IPC {
    const std::string SOCKET_FILE = "hangman_socket.txt";

    const int MAX_MESSAGE_SIZE = 256;
    
    const int LOCK_TIMEOUT = 5000;
    const int READ_TIMEOUT = 10000;
}

#endif