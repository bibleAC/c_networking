# networking

Learning network programming in C on Linux: sockets, a line-based chat protocol, and a poll() server.

## Requirements

- clang 22 or newer (uses the C `defer` TS: `-std=c2y -fdefer-ts`)
- [just](https://github.com/casey/just)

## Build and run

```sh
just run chat_server                     # listens on port 8080
just run client 127.0.0.1 8080 alice     # in another terminal
just run client 127.0.0.1 8080 bob       # and another
```

Type to chat. Ctrl+D leaves.

```sh
just build-all    # build every program in src/
just test         # build and run everything in tests/
just clean
```

## Layout

```
src/
  chat_server.c   chat server, one process, poll()
  client.c        chat client, poll() on stdin and the socket
  server.c        older echo server, one process per client with fork()
lib/
  net_utils.c     send_all()
  protocol.c      line parser and name validation
  server_messages.c   lines and error messages the server sends
tests/            assert-based tests, run with `just test`
PROTOCOL.md       the chat protocol
```

## Protocol

Plain text over TCP, one command per line, lines end in `\n`, max 1024 bytes.
Client sends `NICK`, `MSG`, `QUIT`; server sends `OK`, `ERR`, `MSG`, `JOIN`, `LEAVE`.
See [PROTOCOL.md](PROTOCOL.md).

You can talk to the server by hand:

```sh
nc localhost 8080
NICK alice
MSG hello
```

## Limitations

- Sends are blocking: a client that stops reading can stall the server.
- Up to 64 clients.
- IPv4 only.
- The terminal mixes incoming messages with what you are typing.