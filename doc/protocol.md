Universal Serialization Protocol
================================

Userp is primarily designed to be a "wire protocol" of bytes to transmit Data and Metadata,
though it is defined in more general terms that could but used in many different ways, such
as storing protocol fragments in a database or writing protocol fragments that depend on
pre-negotiated metadata shared between writer and reader.

Userp contains many optimizations to make it suitable for any of the following:

  * Storage of large data packed tightly
  * Storage of data aligned for fast access
  * Storage of data that matches "C" language structs
  * Encoding and decoding in constrained environments, like Microcontrollers
  * Zero-copy exchange of data (memory mapping)
  * Efficiently storing data with repetitive string content
  * Types that can translate to and from other popular formats like JSON

In other words, Userp is equally usable for microcontroller bus messages, audio/video container
formats, or giant trees of structured application data.

Obviously many of these are at odds with eachother, and so tailoring a stream of Userp data to
one of these purposes requires configuration on the encoding end.  A fully implemented decoder
will be able to read the data in any of these arrangements.  (but a partial implementation
of a decoder can easily check whether the data is packed/aligned in the way it expects)

Structure
---------

Userp consists of Data and Metadata.  The Data is consumed directly by the application.
The Metadata is primarily for the decoder implementation, though it may also carry along
metadata needed by the application.

All Data and Metadata are divided into Blocks.  A Block of data is encoded according to the
current state of the protocol that was described by the Metadata.  In a streaming arrangement,
this means that a decoder must process every block of Metadata, though it can skip over Data
blocks.  In a database arrangement, it means that the storage of Data blocks must reference
which Metadata blocks were used.  In more restrictive or custom applications, the encoder and
decoder might be initialized to a state as if they had each processed a Metadata block and then
just exchange data blocks exclusively.  (but I recommend against that design when possible
because it defeats part of the purpose of the protocol)

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
that all blocks are a certain type, in which case the entire data is an encoding of that type,
else the block begins with a declaration of the type of the data followed by the data.

Type System
-----------

The Userp type system is split into two parts: Data Layout and Data Semantics.  Anything the
decoder needs to know about in order to address or unpack the data falls into the category of
Data Layout.  Other metadata about what the data *means* and *how* the application should use
it fall into the category of Data Semantics.

Furthermore, all the type metadata can be described as "Spec Strings".  These are strings of
Userp data but which also happen to be mostly printable ascii characters, and carry all the
data of a type definition, and can be easily added to programs as constants rather than needing
to write out a bunch of awkward static data structures to initialize the library.




