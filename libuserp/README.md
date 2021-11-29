Universal Serialization Protocol C library
==========================================

This library implements Universal Serialization Protocol, which is a binary
format for storing structured data.  It features user-defined types, C struct
compatibility (including alignment), efficient memory usage, speed, and high
compatibility with zero-copy designs.

While the Universal Serialization Protocol is fairly universal and un-bounded,
and could for example, store a petabyte of digits of Pi as a floating point
number, this library introduces some more practical limitations.  It assumes
that any encoded block can fit into main memory, it assumes dynamic memory
allocation is available for things like hash tables and data structures,
and it assumes that you'd like some help keeping track of memory management.

In other words, it is designed for "normal" application development, and might
not be suited for extremely constrained embedded applications, or code that
requires proof of correctness.  The protocol, however, is suitable for
anything and everything.

## API Overview

The entire library operates in the context of a `userp_env`, which holds
various configuration for everything else.  It also holds the last error from
any operation, stored in a `userp_diag` object.

To facilitate zero-copy, the library uses `userp_buffer` objects to describe
each block of data, then references those with `userp_bstr` strings.

Userp's type system allows the protocol to avoid encoding redundant structure
information, and align the data.  This is what sets it apart from ad-hoc
protocols like JSON or BSON.  A `userp_type` primarily describes the
*encoding* of data, but can also be used to describe data semantics via user-
defined metadata.  Types exist in the context of a `userp_scope`.  Each
scope holds a collection of types and a symbol table.  Scopes are nested, with
all things in the outer scope being inherited by the inner scope.

Userp scopes (and the types within) can be encoded into the protocol.  This
is the primary feature that sets it apart from protocols like ASN.1 or Google
Protocol Buffers.  However, the scopes can also be implied in an application
context, allowing you to exchange/store nothing but data buffers.  To this
end, the `userp_enc` and `userp_dec` are separate from the `userp_stream`.

A `userp_stream` is the top-level object for reading and writng the complete
protocol of blocks, which contain scopes, indexes, and metadata.

Anything with a reference count has a corresponding "grab"/"drop" API call
to acquire or release a reference to it.

### Environment Objects

The `userp_env` object holds all configuration needed by the rest of the
library, such as how to allocate memory, how to log things, default options
for the encoders and decoders, and performance tuning options.  There are not
any global variables in libuserp, for thread safety.  A `userp_env` (and any
objects referencing it) must only be used by one thread at a time.

`userp_env` must be dynamically allocated because it is reference-counted, and
only freed after the last thing using it is freed.

### Diagnostic Objects

Rather than simple error codes, linuserp stores various information about the
last error or last logging event that occurred.  You can query the attributes
of these objects to get additional info about the error or message, and you
can format them as strings.

### Buffer Objects and Strings

The `userp_buffer` struct holds information about a buffer, such as how it was
allocated or needs freed, whether it is reference-counted, the reference
count, etc.  You may allocate `struct userp_buffer` objects on the stack or as
members of other structs, as long as you flag them correctly.  Normally, the
library just allocates them on its own.

The `userp_bstr` strings are always dynamically allocated, and hold references
to `userp_buffer` for the underlying data.  `userp_bstr` are not reference
counted, and expected to be allocated, cloned, and freed frequently, because
they are small and copies are inexpensive.

### Scope Objects

A `userp_scope` is a container for types and symbols.  Scopes inherit types
and symbols from a parent scope.  Scopes typically inherit from the standard
root scope of the library, whch defines most of the generic types you would
need.  Scopes are reference-counted, and an inner scope holds a reference to
the parent.

Scopes can be efficiently decoded from a single block of data.  In addition to
decoding them from a protocol stream, you can store them as static resources
of a compiled program to efficiently initialize your encoders or decoders.

### Types and Symbols

`userp_type` and `userp_symbol` are references to data inside of a Scope, and
not first-class objects on their own.  They are valid for as long as the Scope
is valid.

The Userp type system is more about describing the encoding of data than the
semantic of the data.  The main objective is to be able to efficiently encode
any logical type without unnecessary protocol framing.  However, the types are
also generic enough that any conceivable data can be encoded efficiently.
Applications can attach arbitrary metadata to each type, to help pack or
unpack more highly structured application data into simpler compact encodings.

### Encoders and Decoders

`userp_enc` objects represent an encoding context of a `userp_env` and
`userp_scope`, writing to one more more `userp_buffer`.  You call methods on
the encoder to deliver data which will be encoded according to the current
type of element.  In cases where you have verified that a `userp_type` is
identical to the encoding of a C struct, you may pass entire arrays of
C structs to be concatenated directly into the buffer.

`userp_dec` objects represent a decoding context of a `userp_env` and
`userp_scope`, reading from `userp_buffer`s.  As you call methods on the
decoder you will often get back pointers directly to the data of the
underlying `userp_buffer`.

Encoders and decoders hold references to the scopes and blocks that they
operate on, but are not themselves reference counted.

### Streams

The `userp_stream` object handles a series of encoded blocks, each of which
can hold its own embedded scope and other metadata.  When using it to encode
a stream, it generates a header, and then provides `userp_enc` objects for you
to encode each block.  As you complete each block, it tells you which ranges
of your buffer can be written out.  It also gives you the option to include
indexes that allow a decoder to seek within the stream.  When decoding, the
stream reads the header to initialize compatible options to the encoder, and
then you feed it `userp_buffer` data.  It identifies blocks and indexes, and
tracks the scopes, handing you a `userp_dec` for each block.
