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

## Versioning

The `amdrdf` library provides the following forwards/backwards compatibility guarantees:

* For the same major version, any minor version will only *add* new entry points and enumeration values, but existing entry points will not change. Using a higher minor version is always safe. Files created by a newer library *may* not be compatible with older files if new features are used. For example, a new compression codec could be added as part of a minor API change as this is an extension only. However, as long as no new feature is used, all files produced by a newer minor version will remain compatible with older minor versions.
* New major versions *may* add, remove or change entry points. Files created by a newer major version *may* not be compatible with older *major* versions. Files created by an older major version will be supported for *at least* the next higher major version.
* A minor version can deprecate a function, but that function can be only removed in the next major release.

Use `RDF_INTERFACE_VERSION` and `RDF_MAKE_VERSION` to check for the library version.

Patch releases (for example, `1.1.1`) will be bumped for bug fixes and other improvements.

## Changelog

* **1.0**: Initial release
* **1.1**: Improve naming consistency: Add `rdfStreamFromUserStream`, mark `rdfStreamCreateFromUserStream` as deprecated
* **1.1.1**: Fix `rdfChunkFileContainsChunk` returning `rdfResultError` when a chunk was not found instead of `rdfResultOk`
* **1.1.2**:
  * Fix `rdfChunkFileWriterWriteChunk`, `rdfChunkFileWriterEndChunk` returning indices starting at 0 when in append mode instead of counting off the actual contents of the file
  * Fix `rdfChunkFileWriterWriteChunk`, `rdfChunkFileWriterBeginChunk` returning an error when using identifiers of the maximum allowed length (i.e. without a trailing null-terminator.) and a non-zero header pointer
  * Clients can now `#define RDF_CHECK_CALL` before including `amdrdf.h` to customize how errors are handled in the C++ bindings
  * Move constructors in the C++ bindings have been marked as `noexcept`
