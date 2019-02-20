Universal Serialization Protocol
================================

Userp is a binary serialization format that contains its own schema.

Userp combines the generic free-form nature of JSON with the efficient packing
and low-level data access of ASN.1 or Google's Protocol Buffers.  It also
allows room for extra metadata that you can use to store application-specific
details, without breaking compatibility.

The focus for Userp is to be lightweight, flexible enough for any task that
you might use JSON for, and efficient enough for any task that might temp you
to write a custom protocol.
