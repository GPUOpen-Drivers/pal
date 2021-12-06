# RDF

Radeon data file library. Check the [specification](docs/specification.md) for details about the file format.

This repository contains the main library (`amdrdf`) as well as a few useful binaries:

* `rdfm` merges two files together (assuming they contain disjoint data)
* `rdfi` dumps information about a file (what chunks it contains, etc.)
* `rdfg` generates a chunk file from a JSON description (useful for testing)

## Overview

RDF files consist of named chunks. You can think of a file as a list of chunks that you can index using a name. File formats are built on top of RDF by defining chunk names and chunk contents. If you need a new file format, all you need to agree on is a unique set of chunk names and you're good to go. Chunks can be optionally compressed and versioned, which removes the need for versioning whole files.

The same chunk can appear multiple times in a file. They'll get enumerated so it's perfectly fine to have a `Mesh` chunk for each mesh and simply add as often as needed. The full chunk identifier is the chunk name and the index. Chunks are always consecutively numbered in a file.

A chunk consists of a header and the data. The difference between those is that the header is always uncompressed and meant to contain only very little data (they might get all loaded in memory for instance). The data part can be optionally compressed.

There is a C and a C++ API. The C API is the "low level" API that is exported from the DLL/shared object. RDF also comes with a C++ wrapper that uses the C API to simplify usage.

Chunk files are assumed to be immutable, so the code is split into a `ChunkFile` class which represents the (immutable) file, and a `ChunkFileWriter` which can be used to create a new file. Additionally, RDF exposes a stream abstraction to allow reading/writing from disk, memory, or other sources.
