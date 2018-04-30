# tisa-network
Graduate course project on network protocols to develop a reliable communication channel.


_client.c_: client that requests data from server and stores it in a logfile.

_server.c_: server that responds the client with data read from a file.

_lib.h_: library with the functions to implement the system.

*temp_sensor.ino*: reads a temperature sensor and sends data through serial.

_compile_: simply compiles the .c and .h files.

_run_: create the logfiles, concatenates the serial port to a file, and executes the client and server.