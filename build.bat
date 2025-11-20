@echo off
set CXX=g++
set CFLAGS=-Wall -Wextra -std=c++11

echo Building echo server...
%CXX% %CFLAGS% -o server.exe src/server.cpp

echo Building echo client...
%CXX% %CFLAGS% -o client.exe src/client.cpp

echo Build complete!
echo.
echo To test:
echo  1. Run server_basic.exe in one terminal
echo  2. Run client_basic.exe in another terminal
echo  3. Type messages in client