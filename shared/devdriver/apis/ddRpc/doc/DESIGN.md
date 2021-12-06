# ddRpc Library Design

## Audience

This document is intended for DevDriver engineers and engineers with an interest in the design decisions behind the ddRpc API.

## Design Goals

### Move towards industry standard communication models

One of the largest barriers to entry with the existing driver communications solutions is the fact that they use proprietary interfaces. This discourages engineers from other teams from attempting to use the code since its yet another API that they need to learn. Similar to ddNet (which exposes a low-level socket-like interface), ddRpc exposes a remote procedural call interface that potential users of the library may already be familiar with.
 
### Reduce DevDriver library maintenance and protocol iteration time

Many of the production tools supported by the driver are based on rigid hand-written interfaces that live inside the DevDriver library. These interfaces cover both the tool side, and the driver side of the interaction. This unfortunately means that any time a tool needs to change the information sent across the wire, they need to ask the DevDriver team to make a library change. While this process can be relatively quick, it can be error prone and it balloons the number of people that need to be involved in order to implement a simple change.

ddRpc hopes to address these issues by moving the protocol specification outside of the library code so it can be modified without involving the DevDriver team. The library is also paired with a generator tool which automatically creates the error-prone/boilerplate code for the tool side interface, the driver side interface, and the serialization/deserialization layer that goes in between them. If everything is working as designed, adding a new field to an existing protocol should be as simple as adding a line to a protocol file and regenerating both sides of the interface. 

### Shrink the breadth of the interfaces exposed by the DevDriver team

ddRpc is part of the DevDriver team's effort to reduce the surface area of the APIs that we expose. With the original DevDriver library, we weren't very careful about where we put functionality and how it was exposed. This led to a very wide interface that was difficult to understand.

Our recent direction has been to move towards small focused APIs that can be either layered or aggregated to provide higher level functionality. This model retains high flexibility since each component can still be used separately. It also reduces the overall mental burden on users of the APIs since each small piece has a specific and focused purpose. In the future, we'd like everyone to move on top of ddNet and ddRpc (which is implemented on top of ddNet) so that the DevDriver library's interface can be publicly retired.

### Make driver communication accessible to other programming languages

ddRpc like all recent DevDriver libraries is written with a C interface and C++ implementation. This allows us to expose the interface to other languages such as Rust or Python without reducing the portability of the library itself.
