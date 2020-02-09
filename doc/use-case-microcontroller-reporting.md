Use-Case: Microcontroller Reporting
===================================

Suppose you have a tiny microcontroller program that collects data
and only has a few KB of RAM to work with, though many KB of ROM.
A common solution is to write fixed-length records that are just a
coy of the global C structs in the program, but then the receiving
end needs to be aware of the versions of the microcontroller so that
it can correctly decode the data.

Userp can help in this scenario by allowing the microcontroller to
emit a description of the data structure at the start of a stream.
Then, any higher-level program with generic Userp support will be
able to decode the packets.

### Prepare the Metadata

Microcontrollers might not have enough RAM to allow a library to
construct buffered blocks before starting to send them, and there
might not be much processing speed to spare for making entire copies
of all the data.  Userp solves this problem by allowing the metadata
to be stored in ROM, and matching the C struct so that you don't
need to spend any time encoding things at all.

Suppose you have this struct:

    struct my_data {
      uint32_t timestamp;
      uint8_t measurements[14];
    };

The metadata can be generated at compile time from your Makefile:

    stream_header.c: my_data.h
      # this tool doesn't exist yet BTW
      userp-static-meta --bigendian --writer="MySoftware $(VERSION)" \
        --pretty --namespace="comm_" \
        -t my_data my_data.h -o stream_header.c

The result will look like:

    extern const char comm_stream_header[] =
      "UserpS1>" /* Userp Stream Version 1 Big-endian */
      "\x10MySoftware 1.2.3\0" /* Stream writer name */
      "\x06" /* Control Byte: Block with meta and no data, begin scope */
      "\x.." /* block length */
      "\x02" /* string table elem count */
        "\x09" /* strlen of "timestamp" */
        "\x0C" /* strlen of "measurements" */
        "\x07" /* strlen of "my_data" */
      /* string table */
        "timestamp\0measurements\0my_data\0"
      "\x01" /* type table elem count */
        "...."
      "\x00" /* 0 optional metadata fields */
    ;
    extern const char comm_my_data_typecode[1]= "\x40";
    extern const char comm_my_data_block_header[2]=
      "\x01" /* stream control: data payload, no metadata */
      "\x13" /* data payload length */
      "\x40" /* type code for my_data */
    ;

### Initialize a Stream

This next part assumes that the microcontroller offers some sort of
relaible stream transfer, like USB or CAN bus.  For unreliable methods
like serial, you would need to take additional precautions against
corruption, like adding checksums or escaping bytes that are used for
re-synchronizing the stream.

Stream initiation would look something like:

    /* send entire Userp header, defining the my_data type */
    write_bytes(comm_stream_header, sizeof(comm_stream_header));

### Write messages

Now that the peer has seen the metadata block, they understand the direct
memory layout of `struct my_data`, so the only thing left is to construct
blocks, which in this case consist of a stream control byte, payload-length,
type selector, and the bytes of `my_data`.  Since the first three are
constants, they can be copied from ROM.

    struct my_data data;
    ...
    /* write a block of data coming from instance of my_data */
    write_bytes(comm_my_data_block_header, sizeof(comm_my_data_block_header));
    write_bytes(&data, sizeof(data));

If the block were variable length, the program would need code to serialize
Userp "variable quantities" (integer encoded in a varying number of bytes)
and the programmer would need to be aware of further details of the protocol.
