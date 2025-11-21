@echo off
set CXX=g++
set CFLAGS=-Wall -Wextra -std=c++17

echo Building game server...
%CXX% %CFLAGS% -o server.exe src/server.cpp src/protocol/protocol.cpp src/game/game_logic.cpp src/ipc/file_socket.cpp

echo Building game client...
%CXX% %CFLAGS% -o client.exe src/client.cpp src/protocol/protocol.cpp src/game/game_logic.cpp src/ipc/file_socket.cpp

echo Build complete!
echo.
