Universal Serialization Protocol
================================

Userp is a "wire protocol" for efficiently exchanging data, similar in purpose to ASN.1, CBOR,
or Google Protocol Buffers.  The primary design goal is to allow the writer to embed type
definitions within the data such that the data can be written compactly and efficiently, while
still allowing any reader to decode the data without any prior knowledge of the schema.
This allows performance equal or better than ASN.1 or Protocol Buffers, while allowing the
flexibility of CBOR or JSON, and potentially as much utility for type introspection as Json
Schema.

The Userp library is also meant to be a data-coding toolkit rather than just a reader/writer of
streams.  While the normal protocol is a self-contained stream of metadata and blocks of data,
you could also store protocol fragments in a database or exchange protocol fragments over UDP
that depend on pre-negotiated metadata shared between writer and reader.

Userp contains many design considerations to make it suitable for any of the following:

  * Small / Tightly packed data
  * Massive data (the protocol has no upper limits)
  * Data aligned for fast access
  * Data that matches the memory layout of "C" language structs
  * Encoding and decoding in constrained environments, like Microcontrollers
  * Zero-copy exchange of data (memory mapping)
  * Efficiently storing data with repetitive content
  * Types that can translate to and from other popular formats like JSON

In other words, Userp is equally usable for microcontroller bus messages, audio/video container
formats, database row storage, or giant trees of structured application data.

Obviously many of these are at odds with eachother, and so applying Userp to one of these
purposes requires thoughtful definition of the data types.  Planning of the types
should also include consideration for the capabilities of the target; whie the protocol is
un-bounded, implementations can choose sensible maximums, so for example it is a bad idea to
use 65-bit integers if you expect an application without BigInt support to be able to read them.
For embedded applications, if you know that the decoder only has 4K of RAM to work with then it
obviously can't handle 10K of symbol-table from your Record and Enum definitions unless they
are identical to the ones stored on ROM.

Structure
---------

Userp is organized into blocks.  Each block has a length associated with it so that the reader
can process the protocol in large chunks, and possibly skip over the ones it isn't interested
in.  Blocks can define metadata, including a symbol table and type definitions used for the
encoding of its data.  The metadata is followed either by a direct data payload, or a stream
of inner blocks.  The symbols and types declared in the parent block remain in scope for all of
the inner blocks.  The inner blocks may also define their own symbols and types and inner
blocks in a recursive manner.

While the metadata and direct data payload must be contained within a declared number of bytes,
the inner-block payload can act as a stream, with an arbitrary number of inner blocks before
the end of the parent.  Or, the parent can declare an index of inner blocks to help a reader
seek immediately to a point of interest, if the medium is seekable.

The symbol table declares all symbolic (string) identifiers used by the types, as well as any
symbolic constants used in the data.  The symbol table is a series of NUL terminated strings,
so they can be used directly by library code without being copied into separate string objects
in the host language.

The type table declares the structure and minimal metadata of each defined type.  See below for
the details of the type system.  

Following the type table, the block can declare additional arbitrary metadata that the reader
might find useful.  For instance, it can annotate the type definitions to declare character
encodings, indicate versions, or recommend code modules to be used for the decoding.
Applications could also use this for declaring more compreensive schema information than the
type system permits.

Within a direct data payload, the data is arranged into a tree of elements encoded end-to-end
(but with optional alignment or padding).  It is not possible to seek within a data payload
without parsing it, though the length of elements can often be known such that the reader
doesn't need to inspect a majority of the bytes.  For instance, strings are declared with a
byte count, so the reader can skip across strings without scanning for a terminating character
or escape sequences.

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
  * Symbol  - a "friendly" subset of Unicode strings
  * Choice  - a type composed of other types, subsets of types, or enumerated values
  * Array   - a sequence of elements identified by position in one or more dimensions
  * Record  - a sequence of elements identified by name

During encoding/decoding, the API allows you to "begin" and "end" the elements of an array or
record, and to write/read each element as Integer or Symbol.  You may also select a sub-type
for types of "Choice" for any case where "begin", "int" or "symbol" would be ambiguous.
This basic API can handle all cases of Userp data, but common API extensions will handle
native-language conversions like mapping String to an array of integer, mapping float to a
tuple of (sign,exponent,mantissa), and so on, which would be rather inefficient otherwise.

### Standard Attributes

Every type supports these attributes:

  * align - power-of-2 bits to which the value will be aligned.
    Zero causes bit-packing.  3 causes byte-alignment.  5 causes 4-byte alignment, and so on.
  * pad   - number of NUL bytes to follow the encoded value (useful for nul-terminated strings)

### Integer

The base Integer type is the set of integers extending infintiely in the positive and negative
direction.  Integer can be limited by the following attributes:

  * min - the minimum value allowed
  * max - the maximum value allowed
  * bits - forces encoding of the type to N bits
  * names - a dictionary of symbol/value pairs

Any infinite integer type will be encoded as a variable-length quantity.  When 'min' is given,
the variable quantity counts upward from 'min'.  If 'max' is given, it counts downward from
'max'.  If neither are given, the low bit acts as a sign bit.

Finite integer types result from declaring any two of 'bits', 'min' or 'max'.  'bits' alone
results in standard 2's complement encoding, where specifying either 'min' or 'max' causes
unsigned encoding offset from that value.  Fixed-bit-width values can be encoded little-endian
or big-endian based on a flag in the stream header.

When names are given, the resulting integer type can be encoded with the API encode_int *or*
encode_str.  Specifying a symbol that does not exist in `names` is an error.

### Symbol

Symbols in Userp are Unicode strings with some character restrictions which act much like the
symbols in Ruby or the `String.intern()` of Java.  These are meant to represent concepts
with no particular integer value.  Symbols are written into the symbol table, and then any time
they are used in the data they are encoded as a small integer.

### Choice

A choice is composed of one or more Types, Type subsets (for applicable types), or enumerated
values.  When encoded, a Choice type is written as an integer selector that indicates what
comes next.

The main purpose of a choice type is to reduce the size of the type-selector, but it can also
help constrain data for application purposes, or provide data compression by pre-defining
common values.  It can also be used to declare non-integer symbolic enums or subsets of enums.

  * `options` - an array of the Choice's options

Each option is defined by a Choice of:

  * `subtype` - record of `(type, merge_ofs, merge_count)`
  * `value` - encoding of one specific value of any type

The record specifying another type is defined as:

  * `type`        - type reference
  * `merge_ofs`   - starting value within type's initial integer
  * `merge_count` - number of sequential values taken from type's initial integer

To explain the "merge" attributes, imagine that the first option is an integer enum with 7
values, and a second option of Symbol (i.e. any Symbol value).  Without specifying a 'merge',
the value of this Choice would get encoded as one bit (most likely occupying an entire byte)
to select between the two options, and then an encoding of the respective type following that.
But, if you specify `merge_ofs=0` for the first option, then the selector will be the numbers
`0..7` where `0..6` choose one of the 7 values of the integer enum, and `7` selects the second
option to be followed by a Symbol value.  Then, if you set `merge_ofs=0` on the second option
as well, the initial integer component of the Symbol (its length and prefix-present flag) will
merge and the selector is then `0..Inf` where `0..6` are the values of the enum, and `7..Inf`
minus 7 becomes the initial integer component of the Symbol.

### Array

An array is a sequence of values defined by an element type and a list of dimensions.

  * `elem_type` - Choice of Null or a type code
  * `dim` - an array of dimension values, which are Null or positive integer
  * `dim_type` - (optional) the Integer data type to be used to encode Null dimensions

If `elem_type` is Null, it means the element type will be specified in the data at the start
of each value of this type.  If a dimention is Null it likewise means that the element-count
of that dimension will be encoded within the value of this type.  If the length of the `dim`
array is itself 0, that means the number of dimensions will be encoded per-value.

Specifying `dim_type` allows you to control the width of the integers that specify the Null
dimensions.  This allows you to use somehting like Int16u and then have that match a C struct:
```
struct { uint16_t len; char bytes[]; }
```

Because the initial Scope gives you a type for Array with all fields Null, this means you can
write arrays of any other type with arbitrary dimensions without needing to declare
array-of-my-type as a distinct type, in much the same way that C lets you declare an array of
any type by adding one or more `[]` suffixes.

### Record

A record is a sequence of elements keyed by name.  Records can have fixed and dynamic elements,
to accommodate both C-style structs or JSON-style objects, or even a mix of the two.
A record is defined using a sequence of fields.  Each field has a name, type, and Placement.
A Placement is either a numeric offset, `SEQUENCE`, or `OPTIONAL`.  Fields with numeric
placement appear at that bit-offset from the start of the record.  Fields with `SEQUENCE`
placement follow the static portion of the record, end-to-end.  Fields with `OPTIONAL`
placement can be appended to the record as `(key,value)` pairs (but where `key` is a known
symbol, not an encoded string).

A record may then be followed by "ad-hoc" fields, where the name/value are arbitrary, like in
JSON.  Ad-hoc fields encode the field name as a byte array, and are not bound by the
restrictions of being Symbol-friendly unicode.

  * fields - a list of field definitions, each composed of `(Symbol, Type, Placement)`.
  * static_bits - optional declaration of total "static" space in record
  * adhoc - `NONE`, `SYMBOL`, or `STRING`

If `static_bits` is given, there will always be this many bits reserved for the static portion
of the record even if no field occupies it.  This allows for "reserved" fields without needing
to give them a name or spend time decoding them.  Encoders must fill all unused bits with
zeroes.

If `adhoc` is `NONE`, the record doesn't need to include a counter for number of adhoc fields.
If `adhoc` is `SYMBOL`, the keys of the adhoc fields come from the symbol table.  If `adhoc`
is `STRING`, each field name is encoded as an array of bytes, with the data following it.
The bytes *should* be UTF-8, but no guarantees are made and the decoder should use caution
on what it does with these values.  (for instance, they could duplicate another field name,
which is conceptually an error, but not one the library will check for)

Data Language
-------------

Userp data can be described using a lisp-like text notation.  This notation was chosen for ease
of parsing rather than to be human-friendly.  It is used primarily for defining metadata, to
avoid the need to make a bunch of API calls in initialization code.  It can also be encoded to
Userp during the compilation of an application to make statically-defined Metadata Blocks
available as constant data.

### Specificaton

The data language corresponds to the function calls of the Userp library.  Tokens are parsed
and converted to the following API calls:

Regex                           | API Call
--------------------------------|----------------------------------------------------------------
`([-+]?[0-9]+)`                 | encode_int($1)
`#([-+]?[0-9A-F]+)`             | encode_int(parse_hex($1))
`:?$CLEAN_SYMBOL(=?)`           | $2? field($1) : encode_symbol($1)
`:"([^\0-\x20"\(\)]+)"(=?)`     | $2? field(parse_ident($1)) : encode_symbol(parse_ident($1))
`!$CLEAN_SYMBOL($?)(\*([0-9]+)` | $2? encode_type($1) : select_type($1)
`!"([^\0-\x20"\(\)]+)"($?)`     | $2? encode_type(parse_string($1)) : select_type(parse_string($1))
`\(`                            | begin
`\)`                            | end
`'([^\0-\20'\(\)]+)'`           | begin(); for (chars of parse_string($1)) encode_int(char[i]); end()

The `$CLEAN_SYMBOL` regex fragment above is an ASCII alpha character or non-ascii codepoint,
followed by any number of ASCII word characters or period, minus, slash, colon, or any
non-ascii codepoint.

```
CLEAN_SYMBOL = /[_A-Za-z\x80-\x{3FFFF}][-_.:/0-9A-Za-z\x80-\x{3FFFF}]+/
```

### Examples

Description      | Notation                 | API Equivalent
-----------------|--------------------------|-------------------------------
Integer          | 1                        | .int(1)
Negative         | -2                       | .int(-2)
Hex              | #7F                      | .int(0x7F)
Negative hex     | #-7F                     | .int(-0x7F)
Symbol           | abc                      | .sym("abc")
Symbol           | a.b                      | .sym("a.b")
Symbol           | com.example.Foo          | .sym("com.example.Foo")
Awkward symbol   | :"abc%20%22xyz%28%29"    | .sym("abc \"xyz()")
Typed integer    | !Int32+55                | .sel(Int32).int(55)
Typed hex        | !Int32#55                | .sel(Int32).int(0x55)
Typed symbol     | !Errno:FileNotFound      | .sel(Errno).sym("FileNotFound")
Array of Integer | !Int32*(1 2 3)           | .sel(arrayType(Int32,null)).begin(3).int(1).int(2).int(3).end()
Fixed-len Array  | !Int32*3(1 2 3)          | .sel(arrayType(Int32,3)).begin().int(1).int(2).int(3).end()
Array of Any     | (!Int32-22 !Symbol:abc)  | .sel(Array).begin(2).sel(getType("Int32")).int(-22).sel(getType("Symbol")).sym("abc").end()
Record           | !Rect(1 1 10 10)         | .sel(Rect).begin().int(1).int(1).int(10).int(10).end()
Record by name   | !Rect(x=1 y=1 w=10 h=10) | .sel(Rect).begin().field("x").int(1).field("y").int(1)...
Subtype-of-type  | !Employee!EmployeeId+45  | .sel(Employee).sel(EmployeeId).int(45)

Special support for derived types:

Derived Type | Notation                   | API Equivalent
-------------|----------------------------|--------------------------------
String       | !Char*'ABC'                | .sel(arrayType(Char,null)).str("ABC")
(equivalent) | !Char*(65 66 67)           | .sel(arrayType(Char,null)).begin(3).int(65).int(66).int(67).end()
IEEE float   | !Float32+2.5e17            | .sel(Float32).float32(2.5e17);
             | !Float64+2.5e17            | .sel(Float64).float64(2.5e17);
(equivalent) | !Float32( sign=0 exp=17 sig=#200000 ) | .sel(Float32).begin().int(0).int(17).int(0x200000).end()

User Stream Protocol 1
----------------------

Userp can be used in various contexts, one of which is a stream of blocks.  This describes the
"version 1" streaming protocol.

Component       |  Encoding
----------------|--------------------------------------------------------------
Stream          | Header Block [Block...]
Header          | "Userp S1 LE" (or "Userp S1 BE") 0x00 MinorVersion WriterSignature
MinorVersion    | Int16
WriterSignature | 18 bytes containing UTF-8 name and version of writer library
Block           | ScopeID BlockID Length Data
ScopeID         | Integer (variable length)
BlockID         | Integer (variable length)
Length          | Integer (variable length, or can be fixed if changed by Scope)
Data            | Encoded per the block-root-type defined by the Scope.

### Header

Stream mode begins with a magic number identifying the protocol version and endian-ness.
The major version is tied to any change that would break an older implementation's ability to
read the stream.  Less important changes related to metadata or optional features (those which
don't cause undefined behavior for older versions) can be indicated with the MinorVersion which
follows.

A stream can be big-endian (Integers written most significant bit first) or litle-endian
(Integers written least significant byte first).  This is meant to facilitate C-struct
compatibility without placing extra burden on writers.  The flag is part of the header and
applies to the entire stream.

To help identify bugs that might have come from a particular implementation, there is
a string of UTF-8 that follows, where a library can write identifying information about itself.
The string is not required to be NUL-terminated, but must be padded out to 18 characters using
NULs. (bringing the total size of the header to 32 bytes)
There is no standard for the format of this string.

### Per-Block Metadata

The encoding of a block depends on what attributes have been declared to be in effect for the
stream.  The main feature of a block's encoding is a Length (count of bytes) followed by some
value encoded within those bytes.  In the initial Scope, blocks have dynamic Scope and dynamic,
Block ID, so the encoding is ScopeID followed by Block ID followed by Length followed by value
of type Any.

A decoder must check the type of the block to see if it is one of the special types that defines
changes to the encoding of the stream.  If the block is just plain data, it may skip over the
block using the Length to indicate how many bytes to skip.

### Metadata Blocks

The first (and only?) special type of block is the Metadata Block.  A metadata block is defined
as a record of ( Identifiers, Types, BlockRootType, BlockHasID, BlockImpliedScope, BlockIndex ) 
It also allows dynamic fields for other data which does not affect the encoding.

#### Identifiers

This is an array of type 'Ident'.  Each identifier is written as an optional prefix identifier
followed by an array of bytes (which should be valid UTF-8, but isn't enforced).
Every identifier is given the next available ID in the identifier table.  The Identifier table
inherits from a parent Scope to its child.

#### Types

Each type is encoded as the identifier name of the type (optional) followed by a type which it
inherits followed by the type-definition's attributes it wants to override.

Every type gets added to the Choice type named "Any".

#### TypeMetadata

Each type may have additional metadata attached to it.  However, encoding the values for that
metadata might require the type to be defined, so the array of metadata is encoded after the
array of types.  Metadata element N applies metadata to the Nth type of the types array.
The arrays do not need to be the same length, though.

#### BlockRootType

This specifies the type of every block in this scope.  If the type is a choice like 'Any' or
'AnyPublic', then essentially every block defines its own type.  If the type has a known
constant length, then every block is a constant size and no Length will be written between
blocks.

#### BlockHasID

This is a boolean.  If true, every block referencing this scope will also by prefixed by a
Block ID.  If false, all Block IDs are assumed to be 0 and does not need encoded.

#### BlockImpliedScope

This is a boolean.  If true, then all blocks following this metadata block will automatically
belong to this scope, and do not need their ScopeID encoded.  This will continue infinitely
to the end of the stream unless BlockIndex is also given, which limits the number of blocks.

#### BlockIndex

This is an array of N block metadata entries for Blocks that follow this one.  The array is of
type 'BlockMeta', whose fields are affected by the various flags defined previously, but this
also implies BlockImpliedScope=true, meaning every block in this index belongs to this Scope.

If every block would have BlockID and Length, then this is an array of { BlockID, Length }.
At the opposite extreme, if Length is known and BlockHasID is false, the record has no
attributes at all and so the array is nothing but a count of the number of blocks.

This allows a metadata block to contain an index of many following records so that
a reader could skip to the Nth block, if it wanted, or just quickly look up the location of a
block with a specific ID.  The block values are then packed end to end, which could be useful
if all the values needed to be 4K aligned or something.

