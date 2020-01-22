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

The most direct way to encode this would be to create a strem of blocks,
then encode each block by declaring the record type and then writing the
fields using the encding functions.

    /* variable declarations and error checking omitted */
    stream= userp_stream_new(userp_ctx);
    userp_stream_output_to_fd(fd);
    outer_block= userp_stream_new_block(stream, USERP_BLOCK_STREAM);
    scope= userp_block_scope(outer_block);
    type_uint= userp_scope_builtin_type(scope, USERP_TYPE_UINT);
    type_buffer= userp_scope_builtin_type(scope, USERP_TYPE_BYTEARRAY);
    userp_field_def_t fields[]= {
      { .name= "timestamp",  .type= type_uint,   .placement= USERP_SEQ },
      { .name= "frame_type", .type= type_uint,   .placement= USERP_SEQ },
      { .name= "data",       .type= type_buffer, .placement= USERP_SEQ }
    };
    type_videoframe= userp_scope_new_record(scope, "myapp.VideoFrame", 3, fields);
  
    for ( ... ) { /* loop over audio, video, and subtitle frames */
      if ( ... ) { /* current frame is video */
        vframe= ...;
        inner_block= userp_block_new_subblock(outer_block);
        enc= userp_block_encoder(inner_block);
        userp_enc_begin_record(enc, type_videoframe);
        userp_enc_uint64(enc, vframe->timestamp);
        userp_enc_uint8(enc, vframe->frame_type);
        userp_enc_bytearray(enc, vframe->data_len, vframe->data);
        userp_enc_end(enc);
        userp_block_end(inner_block);
      }
  	...
    }
    userp_block_end(outer_block);

During decoding, you read each block fully, then determine what type it is, then decode
into the appropriate structure.  Initializing the stream with a blocking file handle
tkes care of the reading part.  Then you just need to verify that the types you expect
exist in the scope of the first block, then start iterating the subsequent blocks.

    stream= userp_stream_new(userp_ctx);
    userp_stream_input_from_fd(fd);
    block= userp_stream_next_block(stream);
    scope= userp_block_scope(block);
    type_videoframe= userp_scope_find_type(scope, "myapp.VideoFrame");
    if (!type_videoframe || /* further validate whether the type is correct */) {
      printf("Did not find expected myapp.VideoFrame type definition\n");
      abort();
    }
    while (block= userp_stream_next_block(stream)) {
      dec= userp_block_decoder(block);
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

The biggest inefficiency here is that you have to fully copy the video frame's data into and
out of buffers.  Userp offers some options to facilitate zero-copy designs.  The goal during
encoding is to write directly from your vframe->data into the stream's file handle, during
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
