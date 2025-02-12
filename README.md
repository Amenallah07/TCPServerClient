# Multi-Client TCP Server

A high-performance C++ TCP server that handles multiple clients simultaneously using a thread-per-client architecture. 
The server implements the Singleton design patterns.

## Features

- Supports up to 6 concurrent TCP clients
- Generates and broadcasts unique 32-bit sequence numbers every second
- Thread-safe client handling
- Unique ID generation across server restarts
- Broadcasts client count on receiving newline characters
- Graceful shutdown with Ctrl+C "Client will receive Thank you"
- Minimal use of global variables

## Requirements

- Linux operating system
- G++ compiler with C++17 support
- POSIX threads support
- Standard C++ libraries

## Building

To compile the server, run:

```bash
g++ -o server server.cpp -std=c++17 -pthread
```

## Running

Start the server

```bash
./server
```

The server will listen on port 12345 by default.


## Testing

You can test the server using netcat (nc) client. Here are several testing scenarios:

```bash
nc localhost 12345
```

- Open multiple terminals
- Run nc localhost 12345 in each terminal
- Press Enter in any client to see the total number of connected clients

## Contributing
Feel free to submit issues and enhancement requests.
