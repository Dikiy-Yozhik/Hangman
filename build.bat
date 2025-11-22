@echo off
set CXX=g++
set CFLAGS=-Wall -Wextra -std=c++17

echo Creating bin directory...
if not exist bin mkdir bin

echo Building game server...
%CXX% %CFLAGS% -o bin/server.exe ^
  src/server/main.cpp ^
  src/server/game_session.cpp ^
  src/server/session_manager.cpp ^
  src/protocol/protocol.cpp ^
  src/game/game_logic.cpp ^
  src/ipc/file_socket.cpp ^
  src/ipc/file_handle.cpp ^
  src/ipc/file_lock.cpp ^
  src/ipc/region_ops.cpp

echo Building game client...
%CXX% %CFLAGS% -o bin/client.exe ^
  src/client/main.cpp ^
  src/client/game_client.cpp ^
  src/protocol/protocol.cpp ^
  src/game/game_logic.cpp ^
  src/ipc/file_socket.cpp ^
  src/ipc/file_handle.cpp ^
  src/ipc/file_lock.cpp ^
  src/ipc/region_ops.cpp

echo Build complete!
echo Executables are in: bin\
echo.
