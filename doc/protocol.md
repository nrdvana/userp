Universal Serialization Protocol
================================

Userp is a "wire protocol" for efficiently exchanging data, similar in purpose to ASN.1, CBOR,
or Google Protocol Buffers.  Its primary differentiating feature is that it can also encode
type definitions which define the structure of its data, eliminating the need for an
out-of-band data specification to be pre-processed by compiler tools, while also allowing for
more efficient encodings than would be possible with a protocol of more generic structures.

In short, it gives the data flexibility of JSON with the encoding efficiency of ASN.1

The Userp library is also meant to be a data-coding toolkit rather than just a reader/writer of
streams.  While the normal protocol is a self-contained stream of metadata and blocks of data,
you might also want to store protocol fragments in a database or exchange protocol fragments
over UDP that depend on pre-negotiated metadata shared between writer and reader.  The Userp
library makes all of this possible.

Userp's design makes it appropriate for any of the following:

  * Small / Tightly packed data
  * Massive data (the protocol has no upper limits)
  * Data aligned for fast access
  * Data that matches the memory layout of "C" language structs
  * Encoding and decoding in constrained environments, like Microcontrollers
  * Zero-copy exchange of data (memory mapping)
  * Efficiently storing data with repetitive content
  * Mapping to and from other popular formats like JSON

In other words, Userp is equally usable for microcontroller bus messages, audio/video container
formats, database row storage, or giant trees of structured application data.

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
Applications could also use this for declaring more comprehensive schema information than the
type system permits.

Within a direct data payload, the data is arranged into a tree of elements encoded end-to-end
(but with optional alignment or padding).  It is not possible to seek within a data payload
without parsing it, though the length of elements can often be known such that the reader
doesn't need to inspect a majority of the bytes.  For instance, strings are declared with a
byte count, so the reader can skip across strings without scanning for a terminating character
or escape sequences.  Another example, if a record type has a fixed length, then an array of
those records can be accessed by index without needing to parse every record.

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
native-language conversions like mapping native "String" to Userp array of integer, mapping
native "float" to a Userp record of (sign,exponent,mantissa), and so on, which would be rather
inefficient otherwise.

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
  * bswap - an instruction for how to swap bytes when reading or writing this type

Any infinite integer type will be encoded as a variable-length quantity.  When 'min' is given,
the variable quantity counts upward from `min`.  If `max` is given, it counts downward from
`max`.  If neither are given, the low bit acts as a sign bit.  If both are given, or if `bits`
are given, the integer becomes a finite-range type.  `bits` alone results in standard 2's
complement encoding, where specifying `bits` with either `min` or `max` will be encoded as an
unsigned offset in N bits from that value.  Finite-range integers may also have a byte-swap
applied to them, for compatibility with pre-existing protocols.

When names are given, the resulting integer type can be encoded with the API encode_int *or*
encode_str.  Specifying a symbol that does not exist in `names` is an error.

### Symbol

Symbols in Userp are Unicode strings with some character restrictions which act much like the
symbols in Ruby or the `String.intern()` of Java.  These are meant to represent concepts
with no particular integer value.  Symbols are written into the symbol table, and then any time
they are used in the data they are encoded as a small integer which may vary by context of
the data block.  This is what sets them apart from "named integers" (above) which always have
the same integer value, and where the symbol is merely convenience to help select or diagnose
the integer.

### Choice

A choice is composed of one or more Types, Type subsets (for applicable types), or enumerated
values.  When encoded, a Choice type is written as an integer selector that indicates what
comes next.

The main purpose of a choice type is to reduce the size of the type-selector, but it can also
help constrain data for application purposes, or provide data compression by pre-defining
common values.  It can also be used to declare symbolic enumerations.

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
of each array.  If a dimension is Null it likewise means that the element-count of that
dimension will be encoded within each array.  If the length of the `dim` array is itself 0,
that means the number of dimensions will be encoded per-array.

Specifying `dim_type` allows you to control the width of the integers that encode the per-array
dimensions.  This allows you to use somehting like `Int16u` and then have that match a C struct:
```
struct { uint16_t len; char bytes[]; }
```

Because the initial Scope gives you a type for Array with all fields unspecified, this means
you can write arrays of any other type with arbitrary dimensions without needing to declare
array-of-my-type as a distinct type, in much the same way that C lets you declare array
variables of any type by adding one or more `[]` suffixes.

### Record

A record is a collection of fields keyed by name.  Records can have both static-positioned and
stream-encoded elements, to accommodate both C-style structs or JSON-style objects, or even a
mix of the two.  A record is defined by a list of fields, and count of static bytes.  Each
field has a name and type, and optional fields to indicate whether to load it from the static
data, and how to determine whether it is present in a record.

  * `fields` - an array of field definitions, each composed of
    * `name`            - name (Symbol) of field
    * `type`            - type (TypeRef) of field data
    * `extra`           - field will only be present if enumerated as an extra field
    * `static_offset`   - bit (or word) offset from start of static area where field is found
    * `static_wordsize` - power of 2 word size (in bits) which field will be read from
    * `static_byteswap` - indicates how to swap bytes of word before reading
    * `static_shift`    - bit shift of word before reading
    * `static_mask`     - bit mask of word before reading
    * `cond_offset`     - bit (or word) offset from start of static area of "field present" flag
    * `cond_wordsize`   - power of 2 word size (in bits) of boolean flag.  Defait is 1.
    * `cond_mask`       - bit mask of word before checking for true/false
  * `static_bits` - optional declaration of total "static" space in record
  * `other_field_type` - optional Type for ad-hoc 'extra' fields; Null means no ad-hoc fields.

If `static_bits` is given, there will always be this many bits reserved for the static portion
of the record even if no field occupies it.  This allows for "reserved" fields without needing
to give them a name or spend time decoding them.

Records are encoded as a number of bytes of 'extra' field specification. (omitted if there are
no 'extra' fields or ad-hoc fields)  The bytes are decoded into an array of field or symbol
references (and types, for symbol references).  This is followed by the static portion of the
record (to the alignment of the record type), followed by any non-static 'field', followed
by the 'extra fields', if any.

Userp Stream Protocol 1
-----------------------

Userp can be used in various contexts, one of which is a stream of blocks.  This describes the
"version 1" streaming protocol.

Component       |  Encoding
----------------|--------------------------------------------------------------
Stream          | Header Block [Block...]
Header          | "UserpS1\x00" Int16u WriterID
WriterID        | WriterName \x00 [EnvStr...]
EnvStr          | /[^=]+(=.*)/ \x00
Block           | ControlCode BlockLen? Meta? Data?
Meta            | SymbolTable TypeTable [AdHocField...]
Data            | Encoded per the block-root-type

### Header

Stream mode begins with a magic number identifying the protocol version.
The major version is tied to any change that would break an older implementation's ability to
read the stream.

To help identify bugs that might have come from a particular implementation, there is WriterID
that follows, where a library or application can write identifying information about itself.
The length of the WriterID is a 16-bit int (not variable quantity) to make it easy to parse
by tools that don't have full Userp support (like the 'file' command).  The WriterID must be
UTF-8 and include a NUL terminator.  After the NUL, it may include additional NUL-terminated
strings of the form ``NAME=VALUE``.  The total ID is capped at 255 bytes (including NULs) so
any metadata of a larger nature should be encoded as normal Userp metadata on the first block.

### Block

The rest of the stream is a series of Blocks.  A Block is a record pre-defined by the protocol,
or a compatible user-defined record.  The record has one field `data` which exposed to the user
as the official data of the stream, and any number of other fields carrying metadata.  Every field of the block must either be a
fixed-length, array of fixed-length elements, or flagged as `length_delim` so that it can be
skipped.  This allows a parser to always know how many bytes it needs to continue decoding.
A block may declare a type table to define new types, and it may begin a scope in which the
blocks are encoded using a different record definition.

The pre-defined fields of a Block are:

  - id: type=unsigned, placement=often
  - scope: type=BlockScope, placement=often, length_delim=true
    - symbol_table: type=SymbolTable, placement=often
    - type_table: type=TypeTable, placement=often
    - type_meta: type=TypeMeta, placement=seldom
    - block_type: type=typeref, placement=seldom
  - data: type=Any, placement=often, length_delim=true
  - block_index: type=array of blockpointer, placement=often

#### Symbol Table

The symbol table holds a collection of identifiers to be used in the type definitions, data,
or future blocks.  The symbol table may duplicate symbols seen in earlier blocks, but may not
contain duplicates within itself.  The symbol table is encoded as a count of symbols followed
by NUL-terminated strings end-to-end.  The reader must therefore scan the entire string
counting NULs and marking the starts of strings, but this work likely needs performed
regardless (to validate the UTF-8 correctness, and build a hash table) and allows for a very
compact representation.

The pre-defined type SymbolTable is declared as a simple array of bytes.

#### Type Table

The type table defines a list of types which will be appended to the stack of existing types.
Only the name and structure of the types are declared here; any other per-type metadata gets
encoded later.  The decoder must fully process the table before any further decoding on this
block, or later blocks if this block begins a Scope.

The pre-defined type TypeTable is an array of TypeDefinition, which is a choice of each of the
records that could define a type.  The types should be processed immediately, and will be
available for use in subsequent attributes of the parent block record.

#### Ad-hoc Fields

The remainder of the metadata section is a list of ad-hoc fields.  These fields may be encoded
using types and symbols that were just defined in the previous steps.  The ad-hoc fields can be
used by encoders for whatever data they like, but it can also be used for Userp protocol
features like indxing blocks (for random access) or appending metadata to the types in the type
table.

These extra metadata fields may include special keys that have meaning to the protocol, but are
not required in order to parse it.

  - TypeMetadata: generic key/value data attached to any of the types defined in the type table
  - BlockIndex: an index of locations of blocks within this stream.
  - BlockRootType: change the default root type for this block and any others in this Scope
  - BlockID: attach an ID to this block so that other blocks can refer to it.

### Type Definition

Type definitions are simply encoded records that describe a type.

#### Integer Type Definition

  - name        Always     SymbolRef
  - parent      Often      TypeRef
  - align       Often      IntU
  - bits        Often      IntU
  - min         Often      Int
  - max         Seldom     Int
  - bswap       Seldom     ByteSwapType
  - names       Seldom     NamedIntArray
  - pad         Seldom     Padding
  - mask        Seldom     Integer
  - shift       Seldom     Integer

  - name        Always
  - parent      Optional
  - align       Optional   IntU
  - bits        Optional   IntU
  - min         Optional   Int
  - max         Optional   Int
  - bswap       Optional   ByteSwapType
  - 


##### NamedInt

  - name        Always     SymbolRef
  - val_step    Always     Int

#### Record Type Definition

  - name        Always     SymbolRef
  - parent      Often      TypeRef
  - fields      Often      FieldArray
  - static_bits Often      IntU
  - align       Seldom     IntU
  - pad         Seldom     Padding

##### Field Definition:

  - presence    Always     Choice(Always, Often, Seldom, Conditional)
  - name        Always     SymbolRef
  - type        Always     TypeRef
  - offset      Often      Integer
  - bswap       Often      Choice("MSB_FIRST")
  - mask        Often      Integer
  - shift       Often      Integer

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
