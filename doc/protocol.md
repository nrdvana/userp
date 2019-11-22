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
  * pad   - number of NUL bytes to follow the encoded value (useful for nul-terminated strings)

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
on a flag in the stream header.

When names are given, the integer can be encoded with the API encode_int *or* encode_symbol.
Specifying a symbol that does not exist in `names` is an error.

### Symbol

Symbols in Userp are Unicode strings with some character restrictions which act much like the
symbols in Ruby or the `String.intern()` of Java.  These are meant to represent concepts
with no particular integer value.  When encoded, the symbols can make use of a string table in
the metadata block to remove common prefixes.  Thus, they get encoded as small integers rather
than as string literals.

  * `prefix` - a symbol value which must appear at the start of every value of this type

Symbols get encoded as an existing symbol ID and optional suffix (byte array).  If a suffix was
given, it gets added to the temporary Scope of the current Block, enlarging the symbol table.

The combined bytes (prefix with suffix) must be valid UTF-8 codepoints, but the userp
implementation does not apply any transformation to the codepoints and is generally unaware of
any Unicode rules other than what constitutes a valid UTF-8 sequence.

### Choice

A choice is composed of one or more Types, Type sub-ranges (for applicable types), or values.
When encoded, a Choice type is written as an integer selector that indicates what comes next.

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
any type by adding one or more `[]` siffixes.

### Record

A record is a sequence of elements keyed by name.  Records can have fixed and dynamic elements,
to accommodate both C-style structs or JSON-style objects, or even a mix of the two.
A record is defined using a sequence of fields.  Some number of those fields can be
designated as "static", meaning they appear in order at the start of each instance of the
record.  Remaining fields are dynamic, meaning they are encoded as a name/value pair.  (but of
course the name is encoded as a simple integer indicating which field)  A record may then be
followed by "ad-hoc" fields, where the name/value are arbitrary, like in JSON.  Ad-hoc fields
are enabled if either adhoc_name_type or adhoc_value_type is set.

  * fields - a list of field definitions, each composed of `(Symbol, Type)`.
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

