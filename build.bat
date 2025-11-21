@echo off
set CXX=g++
set CFLAGS=-Wall -Wextra -std=c++17

echo Building echo server with protocol...
%CXX% %CFLAGS% -o server.exe src/server.cpp src/protocol/protocol.cpp src/ipc/file_socket.cpp

echo Building echo client with protocol...
%CXX% %CFLAGS% -o client.exe src/client.cpp src/protocol/protocol.cpp src/ipc/file_socket.cpp

echo Build complete!
echo.