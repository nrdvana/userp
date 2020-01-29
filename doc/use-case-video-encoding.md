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
can be represented directly, without unicode escaping.

Encoding looks like this:

    /* variable declarations and error checking omitted */
    stream= userp_stream_writer_new(NULL, 0, 0, 0);
    userp_set_fd(stream, fd);
    for ( ... ) { /* loop over audio, video, and subtitle frames */
      if ( ... ) { /* current frame is video */
        vframe= ...;
        block= userp_block_new(stream, USERP_BLOCK_DATA);
        userp_enc_begin_record(block, NULL);
        
        userp_enc_field_name(block, "timestamp");
        userp_enc_uint64(block, vframe->timestamp);
        
        userp_enc_field_name(block, "frame_type");
        userp_enc_uint8(block, vframe->frame_type);
        
        userp_enc_field_name(block, "data");
        userp_enc_bytearray(block, vframe->data_len, vframe->data);
        
        userp_enc_end(block);
        userp_block_commit(block);
      }
  	...
    }

The structure written to the stream looks like this:

  - Userp Stream Header
  ...
  - Control Code (standalone, has Meta, has Data)
  - Metadata Len
    - Symbol Table count (3)
      - "timestamp"
      - "frame_type"
      - "data"
    - Type Count (0)
    - Extra Attr Count (0)
  - Data Len
    - Root Type Code (Record)
      - Field count (3)
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

    stream= userp_stream_reader_new(NULL);
    userp_stream_reader_set_fd(stream, fd);
    while (block= userp_stream_reader_next_block(stream)) {
      if (userp_type_is_record(userp_dec_current_type(block)) {
        userp_dec_begin_record(block);
        if (!userp_dec_find_field_name(block, "frame_type")
          || !userp_type_is_int(userp_dec_current_type(block)))
          continue;
        userp_dec_int8(block, &frame_type);
        if (frame_type == 'I' || frame_type == 'P' || frame_type == 'B') {
          vframe= myapp_alloc_vframe();
          vframe->frame_type= frame_type;
          userp_dec_field_name(block, "timestamp");   /* fatal error if not found */
          userp_dec_int64(block, &vframe->timestamp); /* fatal if field is not an int */
          userp_dec_field_name(block, "data");        /* fatal if not found */
          len= userp_dec_array_len(block, 0);         /* fatal if not an array */
          myapp_resize_vframe_buf(vframe, len);
          userp_dec_bytearray(block, &vframe->data_len, vframe->data);
        }
        else {
          ...
        }
        userp_dec_end_record(block);
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
the reader must perform a fair amount of type checking for each record it decodes.  Buth of
these problems can be solved by declaring a block that defines custom types, then writing
sub-blocks within that scope.  The symbols and types get declared once, and the decoder can
validate the types once at the beginning of the stream.

The type of the first block is now `USERP_BLOCK_STREAM` to indicate that it begins a series
of nested blocks within the scope of this parent.

    stream= userp_stream_writer_new(NULL, 0, 0, 0);
    userp_set_fd(stream, fd);
    outer_block= userp_block_new(stream, USERP_BLOCK_STREAM);
    type_uint= userp_builtin_type(outer_block, USERP_TYPE_UINT);
    type_buffer= userp_builtin_type(outer_block, USERP_TYPE_BYTEARRAY);
    userp_field_def_t fields[]= {
      { .name= "timestamp",  .type= type_uint,   .placement= USERP_SEQ },
      { .name= "frame_type", .type= type_uint,   .placement= USERP_SEQ },
      { .name= "data",       .type= type_buffer, .placement= USERP_SEQ }
    };
    type_videoframe= userp_new_record(outer_block, "myapp.VideoFrame", 3, fields);
    
    for ( ... ) { /* loop over audio, video, and subtitle frames */
      if ( ... ) { /* current frame is video */
        vframe= ...;
        inner_block= userp_block_new(outer_block);
        userp_enc_begin_record(inner_block, type_videoframe);
        userp_enc_uint64(inner_block, vframe->timestamp);
        userp_enc_uint8(inner_block, vframe->frame_type);
        userp_enc_bytearray(inner_block, vframe->data_len, vframe->data);
        userp_enc_end_record(inner_block);
        userp_block_commit(inner_block);
      }
  	...
    }
    userp_block_commit(outer_block);

The structure of the file will be:

  - Userp Stream Header
  - Block Len
    - Symbol Table count (4)
      - "timestamp"
      - "frame_type"
      - "data"
      - "myapp.VideoFrame"
    - Type Count (1)
      - Definition of myapp.VideoFrame
    - Extra Attr Count (0)
    - Block Root Type
  ...
  - Block Len
    - Block Type (USERP_BLOCK_DATA)
    - Type Count (0)
    - Extra Attr Count (0)
    - Root Type Code (myapp.VideoFrame)
      - Variable length unsigned integer (myapp.VideoFrame.timestamp)
      - Variable length unsigned integer (myapp.VideoFrame.frame_type)
      - Array of 8-bit int (myapp.VideoFrame.data)
        - Elem count of array
        - Bytes of array
  ...

Now, during decoding, you can check the type definitions in the first block to see if you find
something that is an acceptable match for myapp.VideoFrame.  If so, you can skip further
verification each time you encounter that type.

Note that the original decoder example will still work; this one is just faster.

    stream= userp_stream_reader_new(NULL);
    userp_set_fd(stream, fd);
    parent_block= userp_next_block(stream);
    type_videoframe= userp_find_type(block, "myapp.VideoFrame");
    if (!type_videoframe || /* further validate whether the type is correct */) {
      printf("Did not find expected myapp.VideoFrame type definition\n");
      abort();
    }
    while (block= userp_next_block(stream)) {
      type= userp_dec_current_type(dec);
      if (type == type_videoframe) {
        userp_dec_begin(dec);
        vframe= myapp_alloc_vframe();
        /* field decoding needs more error checking unless you verify that
           the record's type will always have the list of fields you care about */
        userp_dec_int64(dec, &vframe->timestamp);
        userp_dec_int8(dec, &vframe->frame_type);
        len= userp_dec_array_len(dec, 0);
        myapp_resize_vframe_buf(vframe, len);
        userp_dec_bytearray(dec, &vframe->data_len, vframe->data);
        userp_dec_end(dec);
      }
      ...
    }
    stream= userp_stream_reader_new(NULL);
    userp_set_fd(stream, fd);
    while (block= userp_next_block(stream)) {
      if (userp_type_is_record(userp_dec_current_type(block)) {
        userp_dec_begin_record(block);
        if (!userp_dec_find_field_name(block, "frame_type")
          || !userp_type_is_int(userp_dec_current_type(block)))
          continue;
        userp_dec_int8(block, &frame_type);
        if (frame_type == 'I' || frame_type == 'P' || frame_type == 'B') {
          vframe= myapp_alloc_vframe();
          vframe->frame_type= frame_type;
          userp_dec_field_name(block, "timestamp");   /* fatal error if not found */
          userp_dec_int64(block, &vframe->timestamp); /* fatal if field is not an int */
          userp_dec_field_name(block, "data");        /* fatal if not found */
          len= userp_dec_array_len(block, 0);         /* fatal if not an array */
          myapp_resize_vframe_buf(vframe, len);
          userp_dec_bytearray(block, &vframe->data_len, vframe->data);
        }
        else {
          ...
        }
        userp_dec_end_record(block);
      }
      ...
    }

## Zero-Copy and Alignment

The biggest inefficiency above is that you have to fully copy the video frame's data into and
out of buffers.  Userp offers some options to facilitate zero-copy designs.  The goal during
encoding is to write directly from your vframe->data into the stream's file handle, and during
decoding, to allow vframe->data to be a pointer into the block's buffer.

One last point of complication is that during decoding you need that vframe->data pointer to
be aligned to some address multiple so that your fancy SSE4 instructions can operate on it.
Userp offers an alignment feature for this, but it must be set on the type as it is encoded
in order to get the desired alignment during decoding.  Also this causes a little bit of
wasted space in the stream.

Suppose you need 32-byte alignment.  Definition of the type would change to:

    type_buffer= userp_scope_builtin_type(scope, USERP_TYPE_BYTEARRAY);
    type_buffer= userp_scope_clone_type(scope, type_buffer);
    userp_type_set_name(type_buffer, "myapp.AlignedBuffer");
    userp_type_set_bytealign(type_buffer, 32);

Then during encoding, let Userp know that it can hold onto your pointer for zero-copy purposes.
You will need to ensure that the pointer remains valid until you call ``userp_block_end``
or ``userp_block_flush``.

    userp_enc_chararray_zerocopy(enc, vframe->data_len, vframe->data);

During decoding, you can ask for the pointer and it will be 32-byte aligned.  The pointer
remains valid as long as the block exists.  By default the block only exists until the next
call to ``userp_stream_next_block`` but you can prevent its destruction by acquiring a
reference using ``userp_block_grab(block)``.  Call ``userp_block_free(block)`` when it
is no longer needed.

    userp_dec_bytearray_zerocopy(dec, &vframe->data_len, &vframe->data);
    vframe->owning_block = block;  /* you'll need this later */
    userp_block_grab(block);

The structure of this particular stream looks like:

  - Userp Stream Header
  - Block Len
    - Block Type (USERP_BLOCK_STREAM)
    - Symbol Table count (5)
      - "myapp.AlignedBuffer"
      - "timestamp"
      - "frame_type"
      - "data"
      - "myapp.VideoFrame"
    - Type Count (1)
      - Definition of myapp.VideoFrame
    - Extra Attr Count (0)
  - Block Len
    - Block Type (USERP_BLOCK_DATA)
    - Type Count (0)
    - Extra Attr Count (0)
    - Root Type Code (myapp.VideoFrame)
      - Variable length unsigned integer (myapp.VideoFrame.timestamp)
      - Variable length unsigned integer (myapp.VideoFrame.frame_type)
      - Array of 8-bit int (myapp.VideoFrame.data)
        - Elem count of array
        - Padding for alignment to multiple of 32 bytes
        - Bytes of array
  - Block Len (repeating for each frame)

