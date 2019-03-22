Universal Serialization Protocol
================================

Userp is a "wire protocol" for efficiently exchanging data.  It serves a similar purpose to
ASN.1 or Google Protocol Buffers, though the design is meant to be more usable for ad-hoc
purposes like JSON or YAML without needing a full type specification pre-processed by a compiler.

The primary design is for streaming data with no pre-defined schema, though the protocol is
built in such a way that it could be used in many different ways, such as storing protocol
fragments in a database or exchanging protocol fragments that depend on pre-negotiated metadata
shared between writer and reader.

Userp contains many design considerations to make it suitable for any of the following:

  * Storage of large data packed tightly
  * Storage of data aligned for fast access
  * Storage of data that matches "C" language structs
  * Encoding and decoding in constrained environments, like Microcontrollers
  * Zero-copy exchange of data (memory mapping)
  * Efficiently storing data with repetitive string content
  * Types that can translate to and from other popular formats like JSON

In other words, Userp is equally usable for microcontroller bus messages, audio/video container
formats, database row storage, or giant trees of structured application data.

Obviously many of these are at odds with eachother, and so applying Userp to one of these
purposes requires thoughtful definition of the data types.  Planning of the types
should also include consideration for the capabilities of the target; the natural limits of
the Userp protocol are far higher than the limits of any given userp implementation, so for
example, it is a bad idea to use 65-bit integers if you expect an application without BigInt
support to be able to read them.  For embedded applications, if you know that the decoder only
has 4K of RAM to work with then it obviously can't handle 10K of symbol-table from your
Record and Enum definitions unless they are identical to the ones stored on ROM.

Structure
---------

Userp consists of Data and Metadata.  The Metadata sets up the types, symbol table, and other
decoding details, and then those types are used to decode the Data.  The Metadata may also
carry application-specific tags attached to data types.

All Data and Metadata are divided into Blocks.  A Data Block is encoded according to the
current state of the protocol that was described by the Metadata.  In a streaming arrangement,
this means that a decoder must process every Metadata Block, though it can skip over Data
Blocks.  In a random-access (i.e. database) arrangement, it means that something must keep
track of which Metadata Block is acting as the scope for each Data Block.  In more restrictive
or custom applications, the encoder and decoder might be initialized to a common Metadata state
(as if they had each processed the same Metadata Block) and then just exchange Data Blocks
exclusively.  (but I recommend against that design when possible because it defeats the ad-hoc
aspect of the protocol)

Data Blocks may contain references to other data blocks.  This provides a basic ability for the
data to form graphs or other non-tree data structures, though not at a very specific level of
detail.  However, a block reference could be combined with an application-specific notation to
form a "symlink" to some data within a block.  For example, an application storing many Nodes
of a directed graph could store an array of nodes in a block, and then encode any reference to
nodes of another block as the block number and the node-array index.  The decoder library would
be responsible for looking up / loading the referenced block, and the application could use the
node index to then re-create the object reference in memory.

The encoding of a block depends on what message framing is available in the particular
application.  For instance, in a stream, the block will be encoded as a length, data payload,
Block ID, and block type indicator.  However if the messages are encoded as datagrams, the
length is known and the payload begins the message.  In a database arrangement the block ID,
type, and length are all known, and only the data payload is present.

Within a block, the data is arranged into a tree of elements.  The Metadata may have declared
that all blocks are a certain type, in which case the entire payload is an encoding of that type,
else the block begins with a declaration of the type of the data followed by the data.

Type System
-----------

The Userp type system is inspired by both ASN.1 and the C language.  The main departure from
ASN.1 is that the Userp type system focuses on the details that affect the encoding of the data,
so that users can quickly put together types that are byte-compatible with C structs. Userp also
marks a clear separation between the details of a type needed to know its encoding, and the
extra metadata details needed by applications.  The encoding must always be understood by the
userp library implementation, but the applications can use metadata to extend the semantics of
a type without breaking compatibility with older decoders.

The Userp type system is built on these fundamental types:

  * Integer - the infinite set of integers, also allowing "named values" (i.e. Enum)
  * Symbol  - an infinite subset of Unicode strings
  * Choice  - a type composed of other types, ranges of types, or constant values
  * Array   - A sequence of elements identified by position in one or more dimensions
  * Record  - A sequence of elements identified by name

During encoding/decoding, the API allows you to "begin" and "end" the elements of an array or
record, and to write/read each element as Integer or Symbol.  You may also select a sub-type
for types of "Choice" for any case where "begin", "int" or "symbol" would be ambiguous.
This basic API can handle all cases of Userp data, but common API extensions will handle
native-language conversions like mapping String to an array of integer, mapping float to a
tuple of (sign,exponent,mantissa), and so on, which would be rather inefficient otherwise.

Each type can be "subclassed" (and in fact, Record and Choice *must* be subclassed) to create
new types.  A "subclass" really just copies all the attributes of the base class and then lets
you override them as needed.

### Standard Attributes

Every type supports these attributes:

  * align - power-of-2 bits to which the value will be aligned, measured from start of block.
    Zero causes bit-packing.  3 causes byte-alignment.  5 causes 4-byte alignment, and so on.
  * pad   - number of 0x00 bytes to follow the encoded value (useful for nul-terminated strings)

### Integer

The base Integer type is the set of integers extending infintiely in the positive and negative
direction.  A subclass of integer can be limited by the following attributes:

  * min - the minimum value allowed
  * max - the maximum value allowed
  * bits - forces encoding of the type to N-bit signed 2's complement
  * names - a dictionary of symbol/value pairs

Any infinite integer type will be encoded as a variable-length quantity, which is specific to
the Userp protocol.  Any integer type with "bits" defined will get an automatic min and max
according to the 2's complement limits for that number of bits.  An integer type with min and
max and without bits will be encoded as an unsigned value of the bits necessary to hold the
value of (max - min).  Fixed-bit-width values can be encode little-endian or big-endian based
on a flag in the metadata block.

When names are given, the integer can be encoded with the API encode_int *or* encode_symbol.
Specifying a symbol that does not exist in names is an error.

### Symbol

Symbols in Userp are Unicode strings with some character restrictions which act much like the
symbols in Ruby or the Stirng.intern() of Java.  These are meant to represent concepts with no
particular integer value.  When encoded, the symbols can make use of a string table in the
metadata block to remove common prefixes.  Thus, they get encoded as small integers rather than
as string literals.

  * values - an array of symbols which compose this type

The Symbol type can be subclassed by giving it a list of values.  Any time this Symbol type is
selected, only those values will be allowed.  The values are of course encoded as integers, but
the integer value is never seen by the application.  This is a "cleaner" way to implement Enums,
though you might still choose an Integer with 'names' in order to be compatible with low-level
code.

Symbols can only be encoded/decoded with encode_symbol/decode_symbol.

### Choice

A choice is composed of one or more Types, Type sub-ranges (for applicable types), or values.
When encoded, a Choice type is written as an integer selector that indicates what comes next.

The main purpose of a choice type is to reduce the size of the type-selector, but it can also
help constrain data for application purposes, or provide data compression by pre-defining
common values.

### Array

An array is a sequence of values defined by an element type and a list of dimensions.  Arrays
do not need to be declared in the Metadata block, and can be given ad-hoc for any defined type.
(this is much like the way that any type in C can have one or more "*" appended to it to become
an array of some number of dimensions)

  * elem_type - the type for each element of the array.  This may be a Choice type.
  * dim - an array of dimension values, which are integers.  Dimensions of 'Null' mean that the
     dimension value will be given within the data right before the values of the sequence.
  * dim_type - the Integer data type to be used when encoding dimensions in the data.
     (default is to use a variable-length integer)
     This allows you to use somehting like Int16s and then have that match a C struct like
	 ```struct { uint16_t len; char bytes[]; }```

### Record

A record is a sequence of elements keyed by name.  Records can have fixed and dynamic elements,
to accommodate both C-style structs or JSON-style objects, or even a mix of the two.
A record is defined using a sequence of fields.  Some number of those fields can be
designated as "static", meaning they appear in order at the start of each instance of the
record.  Remaining fields are dynamic, meaning they are encoded as a name/value pair.  (but of
course the name is encoded as a simple integer indicating which field)  A record may then be
followed by "ad-hoc" fields, where the name/value are arbitrary, like in JSON.  Ad-hoc fields
are enabled if either adhoc_name_type or adhoc_value_type is set.

  * fields - a list of field definitions, each composed of ``(Symbol, Type)``.
  * static_fields - a count of fields which are encoded statically.
  * adhoc_name_type - optional type of Symbol whose names can be used for ad-hoc fields
  * adhoc_value_type - optional type of value for the ad-hoc fields

Data Language
-------------

Userp data can be described using a lisp-like text notation.  This notation was chosen for ease
of parsing rather than to be human-friendly.  It is used primarily for defining metadata, to
avoid the need to make a bunch of API calls in initialization code.  It can also be encoded to
Userp during the compilation of an application to make statically-defined Metadata Blocks
available as constant data.

The data language corresponds to the function calls of the Userp library.  Tokens are parsed
and converted to the following API calls:

Regex                        | API Call
-----------------------------|----------------------------------------------------------------
([-+]?[0-9]+)                | encode_int($1)
#([-+]?[0-9A-F]+)            | encode_int(parse_hex($1))
:?$CLEAN_SYMBOL(=?)          | $2? field($1) : encode_symbol($1)
:"([^\0-\x20"]+)"(=?)        | $2? field(parse_ident($1)) : encode_symbol(parse_ident($1))
!$CLEAN_SYMBOL($?)           | $2? encode_type($1) : select_type($1)
!"([^\0-\x20"]+)"($?)        | $2? encode_type(parse_string($1)) : select_type(parse_string($1))
\(                           | begin
\)                           | end

The CLEAN_SYMBOL regex fragment above is the set of ASCII "word" characters, period, minus,
slash, colon, and ANY upper non-ascii codepoint which begins with a letter or high unicode.

    CLEAN_SYMBOL = /[A-Za-z\x80-\x{3FFFF}][-.:/0-9A-Za-z\x80-\x{3FFFF}]+/
