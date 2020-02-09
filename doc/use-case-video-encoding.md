Use-Case: Vieo Encoding
=======================

Suppose you want to encode video with Userp.  A simple video file probably consists of
a stream of Video, Audio, and Subtitle blocks, each compressed with a fancy algorithm.
The video might additionally be broken into I-frames, P-frames, and B-frames, and the
subtitles and audio might have a stream id to separate different languages.

Suppose that resolves to these simplfied C structs:

    struct VideoFrame {
      uint64_t timestamp;
      char frame_type; # 'I', 'P', or 'B'
      size_t data_len;
      char *data;
    };
    struct AudioFrame {
      uint64_t timestamp;
      int durtion;
      int channel;
      size_t data_len;
      char *data;
    };
    struct Subtitle {
      uint64_t timestamp;
      int duration;
      float x, y;
      int x_align, y_align;
      size_t len;
      char *text;
    };

## The Simple JSON-like Approach

Userp doesn't require you to declare custom types; you can just encode a record of field
names with values like you do in JSON, and it will be more efficient because binary data
can be represented directly, without unicode escaping.  Also, because you get the benefit
of a string table for repeated field names.

Encoding looks like this:

    /* variable declarations and error checking omitted */
    stream= userp_writer_new(NULL, 0, 0, 0);
    userp_writer_set_fd(stream, fd);
    for ( ... ) { /* loop over audio, video, and subtitle frames */
      if ( ... ) { /* current frame is video */
        vframe= ...;
        block= userp_writer_new_block(stream, 0);
        enc= userp_block_get_encoder(block);
        userp_enc_begin_record(enc, NULL);
        
        userp_enc_field_name(enc, "timestamp");
        userp_enc_uint64(enc, vframe->timestamp);
        
        userp_enc_field_name(enc, "frame_type");
        userp_enc_uint8(enc, vframe->frame_type);
        
        userp_enc_field_name(enc, "data");
        userp_enc_bytearray(enc, vframe->data_len, vframe->data);
        
        userp_enc_end_record(enc);
        userp_writer_commit_block(stream, block);
      }
  	...
    }

The structure written to the stream looks like this:

  - Userp Stream Header

...

  - Control Code (has Meta, has Data)
  - Block Len
    - Symbol Table count (3) and lengths (9, 10, 4)
      - "timestamp"
      - "frame_type"
      - "data"
    - Type Count (0)
    - Extra Meta Field Count (0)
    - Type selector (Record)
      - Extra Field count (3)
      - Field Name (sym 0)
      - Variable-length unsigned integer (timestamp)
      - Field Name (sym 1)
      - Variable-length unsigned integer (frame_type)
      - Field Name (sym 2)
      - Array of 8-bit int (data)
        - Elem count of array
        - Bytes of array

...

The decoder would look like this:

    stream= userp_reader_new(NULL);
    userp_reader_set_fd(stream, fd);
    while (block= userp_reader_next_block(stream)) {
      dec= userp_block_get_decoder(block);
      if (userp_type_is_record(userp_dec_current_type(dec)) {
        userp_dec_begin_record(dec);
        if (!userp_dec_field_name(dec, "frame_type")
          || !userp_type_is_int(userp_dec_current_type(dec)))
          continue;
        userp_dec_int8(dec, &frame_type);
        if (frame_type == 'I' || frame_type == 'P' || frame_type == 'B') {
          vframe= myapp_alloc_vframe();
          vframe->frame_type= frame_type;
          userp_dec_field_name(dec, "timestamp");
          userp_dec_int64(dec, &vframe->timestamp); /* fatal if field is not an int */
          userp_dec_field_name(dec, "data");
          len= userp_dec_array_len(dec, 0);         /* fatal if not an array */
          myapp_resize_vframe_buf(vframe, len);
          userp_dec_bytearray(dec, &vframe->data_len, vframe->data);
        }
        else {
          ...
        }
        userp_dec_end_record(dec);
      }
      ...
    }

There are several ways to write the decoding, depending on how extensively you want to check
for errors.  Userp has some methods that let you explore the type of the data, and others that
cause fatal errors if the data is not of an appropriate type.  "Fatal errors" can be directed
to a callback of your choice, giving the option for `abort`, `longjmp`, or just having the call
return 0 or NULL.  In this example, the code checks for a record having a field named
"frame_type" and that `frame_type` is an integer.  It then assumes everything is good and lets
the library catch any broken assumtions as fatal errors.

## Optimization using Custom Types

In the JSON-like approach, the symbol names of fields get re-encoded into every Block.  Also,
the reader must perform a fair amount of type checking for each record it decodes.  Both of
these problems can be solved by declaring a block that defines custom types, then writing
sub-blocks within that scope.  The symbols and types get declared once, and the decoder can
validate the types once at the beginning of the stream.

The block is created with the `USERP_BLOCK_BEGIN_SCOPE` flag to indicate that it must remain
in memory even after it has been read/written until the end of its scope.

    stream= userp_writer_new(NULL, 0, 0, 0);
    userp_writer_set_fd(stream, fd);
    outer_block= userp_writer_new_block(stream, NULL, USERP_BLOCK_BEGIN_SCOPE);
    enc= userp_block_get_encoder(outer_block);
    scope= userp_enc_get_scope(enc);
    type_uint= userp_scope_builtin_type(scope, USERP_TYPE_UINT);
    type_buffer= userp_scope_builtin_type(scope, USERP_TYPE_BYTEARRAY);
    userp_field_def_t fields[]= {
      { .name= "timestamp",  .type= type_uint,   .placement= USERP_SEQ },
      { .name= "frame_type", .type= type_uint,   .placement= USERP_SEQ },
      { .name= "data",       .type= type_buffer, .placement= USERP_SEQ }
    };
    type_videoframe= userp_scope_declare_record(scope, "myapp.VideoFrame", 3, fields);
    userp_writer_commit_block(stream, outer_block);
    
    for ( ... ) { /* loop over audio, video, and subtitle frames */
      if ( ... ) { /* current frame is video */
        vframe= ...;
        inner_block= userp_writer_new_block(stream, outer_block, 0);
        enc= userp_block_get_encoder(inner_block);
        userp_enc_begin_record(enc, type_videoframe);
        userp_enc_uint64(enc, vframe->timestamp);
        userp_enc_uint8(enc, vframe->frame_type);
        userp_enc_bytearray(enc, vframe->data_len, vframe->data);
        userp_enc_end_record(enc);
        userp_writer_commit_block(stream, inner_block);
      }
  	...
    }

The structure of the file will be:

  - Userp Stream Header
  - Control Code (has meta, begin scope)
  - Block Len
    - Symbol Table count (4) and lengths (9, 10, 4, 16)
      - "timestamp"
      - "frame_type"
      - "data"
      - "myapp.VideoFrame"
    - Type Count (1)
      - Definition of myapp.VideoFrame
    - Extra Metadata Field Count (0)

...

  - Control Code (has data)
  - Block Len
    - Type selector (myapp.VideoFrame)
      - Variable length unsigned integer (myapp.VideoFrame.timestamp)
      - Variable length unsigned integer (myapp.VideoFrame.frame_type)
      - Array of 8-bit int (myapp.VideoFrame.data)
        - Elem count of array
        - Bytes of array

...

Now, during decoding, you can check the type definitions in the first block to see if you find
something that is an acceptable match for myapp.VideoFrame.  If so, you can skip further
verification each time you encounter that type.

Note that the original decoder example will still work on this stream that uses custom types;
this code is just faster.

    stream= userp_reader_new(NULL);
    userp_reader_set_fd(stream, fd);
    parent_block= userp_reader_next_block(stream);
    scope= userp_dec_get_scope(userp_block_get_decoder(parent_block));
    type_videoframe= userp_scope_find_type(scope, "myapp.VideoFrame");
    if (!type_videoframe || /* further validate whether the type is correct */) {
      printf("Did not find expected myapp.VideoFrame type definition\n");
      abort();
    }
    while (block= userp_reader_next_block(stream)) {
      dec= userp_block_get_decoder(block);
      type= userp_dec_current_type(dec);
      if (type == type_videoframe) {
        userp_dec_begin_record(dec);
        vframe= myapp_alloc_vframe();
        /* field decoding needs more error checking unless you verify that
           the record's type will always have the list of fields you care about */
        userp_dec_int64(dec, &vframe->timestamp);
        userp_dec_int8(dec, &vframe->frame_type);
        len= userp_dec_array_len(dec, 0);
        myapp_resize_vframe_buf(vframe, len);
        userp_dec_bytearray(dec, &vframe->data_len, vframe->data);
        userp_dec_end_record(dec);
      }
      ...
    }

## Zero-Copy and Alignment

The biggest inefficiency above is that you have to fully copy the video frame's data into and
out of buffers.  Userp offers some options to facilitate zero-copy designs.  The goal during
decoding is to allow vframe->data to be a pointer into the block's buffer (which presumably
could have been memory-mapped directly from some disk device) and the goal during encoding is
to have Userp provide the actual memory-mapped destination buffer to the video decoder.
An alternative "less-copy" design for the encoder is for Userp to hold onto your vframe->data
pointer until is is time to be written to the file handle, rather than first copying to an
internal buffer.  Userp offers an API for all three of these.

One last point of complication is that you need that vframe->data pointer to be aligned to some
address multiple so that your fancy SSE4 instructions can operate on it.
Userp offers an alignment feature for this; just specify a power-of-two alignment on any custom
type, and when it is encoded the stream will insert NUL bytes to reach that alignment (measured
from start of the stream) before encoding the value.

Suppose you need 32-byte alignment.  Definition of the type would change to:

    type_buffer= userp_scope_builtin_type(scope, USERP_TYPE_BYTEARRAY);
    type_buffer= userp_scope_clone_type(scope, type_buffer);
    userp_type_set_name(type_buffer, "myapp.AlignedBuffer");
    userp_type_set_bytealign(type_buffer, 32);

To get true zero-copy during encoding, you need to encode onto your own buffers (which are
presumably a memory-mapped file).  Then, you need to have fully prepared all your metadata
(including strings that will get added to the block's string table) so that there is a concrete
starting address for the value.  Then:

    /* receive pointer to buffer into vframe->data, with at least
       reserve_data_len bytes available for writing. */
    userp_enc_bytearray_zerocopy_prepare(enc, reserve_data_len, &vframe->data);
    
    /* populate vframe->data, discovering the total encoded size of the data */
    vframe->data_len= ...
    
    /* inform Userp how many bytes were actually filled */
    userp_enc_bytearray_zerocopy_commit(enc, vframe->data_len);

For the alternate "less-copy" design, you can ask Userp to use your buffer directly when
it gets to the phase where it writes the completed block:

    userp_enc_bytearray_using_buffer(enc, vframe->data_len, vframe->data);

During decoding, you can ask for the pointer and it will be 32-byte aligned.  The pointer
remains valid as long as the block exists.  By default the block only exists until the next
call to ``userp_reader_next_block`` but you can prevent its destruction by acquiring a
reference using ``userp_block_grab(block)``.  Call ``userp_block_drop(block)`` when it
is no longer needed.

    userp_dec_bytearray_zerocopy(dec, &vframe->data_len, &vframe->data);
    userp_block_grab(block);       /* keep a strong reference */
    vframe->owning_block = block;  /* you'll need this later */

The structure of the aligned stream looks like:

  - Userp Stream Header
  - Control Code (has meta, begin scope)
  - Block Len
    - Symbol Table count (5) and lengths (19, 9, 10, 4, 16)
      - "myapp.AlignedBuffer"
      - "timestamp"
      - "frame_type"
      - "data"
      - "myapp.VideoFrame"
    - Type Count (1)
      - Definition of myapp.VideoFrame
    - Extra Metadata Field Count (0)

...

  - Control Code (has data)
  - Block Len
    - Type Selector (myapp.VideoFrame)
      - Variable length unsigned integer (myapp.VideoFrame.timestamp)
      - Variable length unsigned integer (myapp.VideoFrame.frame_type)
      - Array of 8-bit int (myapp.VideoFrame.data)
        - Elem count of array
        - Padding for alignment to multiple of 32 bytes
        - Bytes of array

...

