# ddSocket (Sockets over DevDriver)

ddSocket is a low-level C API that provides TCP socket-like functionality via the DevDriver communication platform. It is primarily intended to be used as an implementation building block for higher level libraries such as ddRpc and ddEvent. However, it can also be used directly if your communication requirements are complex enough that they require full control of the underlying network byte streams.

## API

### Overview

The majority of API functions interact with a `DDSocket` object in some way.
There's two main subsets of the API, one for client usage, one for server.
There's also a shared set of functions that work on either type of socket.

### Adaptations for DevDriver Platform

A few minor modifications were made to the POSIX socket interface in order to make it work with the DevDriver communication platform.

- DevDriver Client Id <-> IP Address
- DevDriver Protocol Id <-> Network Port

### Client Interface

- Connection
  - A connection to a remote server is acquired with `ddSocketConnect`. This creates a new socket object.

### Server Interface

- Listening
  - `ddSocketListen` is used to begin accepting client connections using the specified protocol
  - `ddSocketAccept` is used to transform an incoming connection into a `DDSocket` object. The server code can interact with this object using the shared interface functions.

### Shared Interface

- Data Transfer
  - Data can be transferred by calling `ddSocketSend` or `ddSocketReceive` on a `DDSocket`
- Socket Destruction
  - `ddSocketClose` is used to destroy a socket object once the application is finished with it

## Background

The DevDriver library was originally created to provide remote access to driver functionality for tooling purposes. Over time, the needs of the tools became more sophisticated and the library grew far beyond its original scope. Implementing new tool functionality with the library became a full time job due to the complex requirements of the applications on top of it and the way the library was designed. It eventually became clear that this process was unsustainable. The DevDriver team didn't have the resources to support the ever growing catalog of driver based tools and the process was becoming a bottleneck.

From there, the team stepped back and attempted to come up with a solution that would scale. The result of this work was the ddSocket API. ddSocket is designed as a general purpose interface and it intentionally avoids anything that isn't related to transferring streams of bytes between tools and drivers. The library should not significantly increase in size as new tools are added and its expected the remain **mostly** the same for the foreseeable future.
