# HTTP/1.1 Multiplexing Webserver in C

This repository contains a simple HTTP/1.1 multiplexing webserver implementation in pure C. The server uses non-blocking I/O and the epoll API for efficient handling of multiple concurrent connections.

## Features

- HTTP/1.1 protocol support
- Multiplexing using epoll for handling multiple connections
- Support for GET and POST methods
- Basic file operations (read and write)
- Custom request parsing and response handling
- Support for various HTTP status codes (200 OK, 201 Created, 400 Bad Request, 404 Not Found)

## Endpoints

The server supports the following endpoints:

1. `/`: Returns a 200 OK response
2. `/echo/<message>`: Echoes back the provided message
3. `/user-agent`: Returns the User-Agent header from the request
4. `/files/<filename>`: Supports GET (to read a file) and POST (to write a file) operations

## Building and Running

To compile the server, use a C compiler such as gcc:

```bash
gcc -o server server.c
```

To run the server:

```bash
./server
```

The server will start and listen on port 4221.

## Dependencies

This project uses standard C libraries and POSIX APIs. No external dependencies are required.

## Configuration

- The server listens on port 4221 by default
- File operations use the `/tmp/data/` directory for reading and writing files

## Notes

- The server uses non-blocking I/O and the epoll API for efficient handling of multiple connections
- The implementation includes basic error handling and logging
- The code demonstrates low-level socket programming, HTTP request parsing, and response construction

## Future Improvements

- Refactor the code for better modularity and readability
- Multi-threading or asynchronous I/O for handling multiple connections
- Thread pool for handling multiple connections
- In-thread multiplexing using epoll
- Add support for more HTTP methods
- Implement better error handling and logging
- Add configuration options (e.g., port number, file paths)
- Implement security features and input validation
- Add support for HTTPS

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Feel free to open an issue or submit a pull request if you find a bug or want to add a new feature.
