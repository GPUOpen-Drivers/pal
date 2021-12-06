meta:
  id: rdf
  title: Radeon Data File
  file-extension: rdf
  endian: le
seq:
  - id: header
    type: header
types:
  header:
    seq:
    - id: identifier
      # This is 8 characters long, hence the trailing space
      # Earlier versions used RTA_DATA which used all 8
      # characters (and it also allows reading directly into
      # an UINT64)
      contents: "AMD_RDF "
    - id: version
      type: u4
    - id: reserved
      type: u4
    - id: index_offset
      type: s8
    - id: index_size
      type: s8
    instances:
      index:
        pos: index_offset
        size: index_size
        type: index
  index:
    seq:
    - id: entries
      type: index_entry
      repeat: eos
  index_entry:
    seq:
    - id: identifier
      type: str
      size: 16
      terminator: 0
      encoding: UTF-8
    - id: compression
      type: u1
      enum: compression
    - id: reserved
      size: 3
    - id: version
      type: u4
    - id: chunk_header_offset
      type: s8
    - id: chunk_header_size
      type: s8
    - id: chunk_data_offset
      type: s8
    - id: chunk_data_size
      type: s8
    - id: uncompressed_chunk_size
      type: s8
    instances:
      header:
        io: _root._io
        pos: chunk_header_offset
        size: chunk_header_size
      data:
        io: _root._io
        pos: chunk_data_offset
        size: chunk_data_size
enums:
  compression:
    0: none
    1: zstd