/* Get the tree-depth of the current node.

The root node of a block has a depth of 0.  If the root node is an array or record, the elements
within will have a depth of 1, and so on.

*/
int userp_dec_node_depth(userp_dec dec);

/* Get the type of the current node or its parents, optionally inspecting Choice sub-types.

If the `parent_level` is zero, this returns the type of the current node.  If parent_level is
1, it returns the type of the record or array (or choice wrapping one of those) that this node
was contained in, and so on.

If the current node is a Choice type, specify `choice_subtype = 1` to inspect the member type
that was actually encoded.  If that was also a Choice type, keep incrementing `choice_subtype`
until it isn't to find out the real type that you will actually be decoding next.

If the `parent_level` or `choice_subtype` are out of bounds, this simply returns NULL and no
error is raised.  If you have finished iterating the elements of an array or record, then
there is no current node and this can also return NULL for `parent_level = 0`.

*/
userp_type userp_dec_node_type(userp_dec dec, int parent_level, int choice_subtype);

/* Get the number of array dimensions in the current node, or a parent.

If the current node is an array (meaning you haven't called begin_array yet, else the current
node would be whatever the first element of the array was) this returns the number of declared
dimensions for the array.  Most arrays will be 1-dimensional.

Note that you don't have to know the dimensions of an array in order to decode it; you can
count the elements as a flat list using `userp_dec_node_elem_count(dec, 0, -1)`.

*/
size_t userp_dec_node_dimensions(userp_dec dec, int parent_level);

/* Get the number of elements in a record or array or array dimension.

If the node at `parent_level` (where `parent_level=0` is the current node) is an record,
this returns the number of fields present for it.  If the node is an array, this returns
the element count for one of its dimensions.  Specifying a dimension of -1 will return the
total number of elements for all dimensions.

*/
size_t userp_dec_node_elem_count(userp_dec dec, int parent_level, int dimension);

/* Get the current iteration offset within a parent element.

If the node at `parent_level` is an array, this returns how far along through the elements
of the array you have iterated.  In other words, the loop index variable.

If the node is a record, it refers to the offset of the current field out of the fields
which are present in this encoding of the record.

This always returns zero if `parent_level` is zero, because the current node cannot be
iterated without it becoming a parent.

*/
size_t userp_dec_node_elem_idx(userp_dec dec, int parent_level);

/* Get the field name of a node within its parent record.

If the `parent_level` is zero, and the current node is a field of a record, this returns the
field name.  Likewise it for higher parent_level values.  The root node always had a field
name of NULL.

*/
userp_symbol userp_dec_node_name(userp_dec dec, int parent_level);

/* Decode the current node as an integer, and move to the next node if successful.

If the current node is an integer-compatible type and fits within an `int`, this stores
the value into `*out` and moves the internal iterator to the next node, and returns true.
If one of those pre-conditions is not true, this returns false and sets an error flag.

*/
bool userp_dec_int(userp_dec dec, int *out);

/* Decode the current node as a Symbol, and move to the next node if successful.

If the current node is a symbol-compatible value (such as plain Symbol or an enumerated
integer value) return the Symbol.  Else returns NULL and sets an error flag.

*/
userp_symbol userp_dec_symbol(userp_dec dec);

/* Decode the current node as a Type reference, and move to the next node if successful.

If the current node is a reference to a type, return the type and advance to the
next node.  Else it returns NULL and sets an error flag.

*/
userp_type userp_dec_typeref(userp_dec dec);

/* Begin iterating the array elements of the current node.

If the current node is not an array, this returns false and sets an error flag.

For convenience, you may pass a `size_t` pointer which will receive the total number
of elements in the array, the same value returned by `userp_dec_node_elem_count(dec,0,-1)`.
The size_t will only be written if it is not NULL and if opening the array succeeds.

*/
bool userp_dec_begin_array(userp_dec dec, size_t *n_elem);

/* Reset array iteration back to the first element.

If the parent node of the current node is an array, reset the current node to the
first element of that array.  If the immediate parent node is not an array, this
returns false and sets an error flag.

*/
bool userp_dec_rewind_array(userp_dec dec);

/* Stop iterating an array and set the current node to the element following the array.

Assuming the parent node is an array, set the current node back to that array and then
advance to the element following it, if any.  You are not required to have iterated all
the elements in the array.

*/
bool userp_dec_end_array(userp_dec dec);

/* Begin iterating the record fields of the current node.

If the current node is a record, it becomes the parent node and the first field
present in this encoding of the record becomes the current node.  As a convenience,
you can pass a pointer to a `size_t` that will receive the number of fields present
in the record.  This is the same value reported by `userp_dec_node_elem_count(dec,0,0)`

If the current node is not a record, this returns false and sets an error flag.

*/
bool userp_dec_begin_record(userp_dec dec, size_t *n_elem);

/* Reset iteration of a record back to the first field.

If the parent node of the current node is a record, this resets the current node
to the first field present in this encoding of the record.

*/
bool userp_dec_rewind_record(userp_dec dec);

/* Seek to the specified field of the current record.

If the parent node is a record and has this field encoded, the current node is set to
that field.  If the record does not have the field, this returns false and sets a
"soft" error flag indicating so.  If the parent element is not a record, this returns
false and sets a "hard" error flag that needs to be cleared.

*/
bool userp_dec_seek_field(userp_dec dec, userp_symbol field);

/* Stop iterating a record and continue with the element following it, if any.

If the parent node is a record, this ends the iteration of that record returning
it back to the current node, and then advances to the node following it, if any.
You do not need to iterate all the fields of a record before ending the iteration.

*/
bool userp_dec_end_record(userp_dec dec);

/* Decode the current node as a `float` and move to the next node.

If the current node is a floating-point record that fits in `float`, or is an integer
which can be losslessly loadded into a `float`, this stores the value into `*out` and
returns true.  Else it returns false and sets an error flag.

*/
bool userp_dec_float(userp_dec dec, float *out);

/* Decode the current node as a `double` and move to the next node.

If the current node is a floating-point record that fits in `double`, or is an integer
which can be losslessly loadded into a `double`, this stores the value into `*out` and
returns true.  Else it returns false and sets an error flag.

*/
bool userp_dec_double(userp_dec dec, double *out);

/* Decode the current node as an array of bytes, and move to the next node.

If the current node is an array of integers that fit into bytes, and if the supplied
buffer is large enough to hold them (according to the initial value of `length_inout`),
this copies the array into the supplied buffer and updates `length_inout` to the actual
length of the data.

If the buffer is too small or the current node cannot be represented as an array of
bytes, this returns false and sets an error flag.

*/
bool userp_dec_bytes(userp_dec dec, void *out, size_t *length_inout);

/* Decode the current node as a NUL-terminated string.

If the current node is an array of integers that fit into bytes, and if none of those
integers are zero, and if the supplied buffer is large enough to hold them *and* a
NUL terminator, this copies them into the supplied buffer and updates `length_inout`
to the number of characters (not including the NUL)

*/
bool userp_dec_string(userp_dec dec, char *out, size_t *length_inout);

/* Store a record's static-placement fields into a compatible struct buffer.

In order to safely perform this, a user should first VERIFY THE TYPE of the
current node against a Type that was generated at compile time from the struct
definition.  To guard against lazy API users, this function takes an argument
of the `sizeof` the struct the user is about to fill, and verifies that it
matches the number of bytes of static data in the record.  (the record may
contain additional dynamic fields which will be ignored)

As a special case for uses like

  struct { int a, b, c, d;  int len; int array[]; }

if the fixed-length portion of the record is immediately followed by an array
field which has a fixed-length element count, then the size of the static
record plus the size of the array length will be checked against the `sizeof`
the struct, and if it matches, the static portion *and the array that follows
it* will be copied into the user buffer.  `length_inout` will be updated to the
number of bytes copied.

*/
bool userp_dec_struct(userp_dec dec, size_t sizeof_struct, void *out, size_t *length_inout);

/* Get a pointer to a record directly out of the stream buffers.

This operates identical to `userp_dec_struct`, but it returns a pointer directly
to the bytes of the source buffer.  See the important notes about the zerocopy API.

*/
userp_buffer_ranges userp_dec_struct_zerocopy(userp_dec dec, size_t expected_sizeof);

/* Get a pointer to an array directly out of the stream buffers.

This operates identical to `userp_dec_array`, but returns a pointer directly
to the bytes of the source buffer.  See the important notes about the zerocopy API.

*/
userp_buffer_ranges userp_dec_bytes_zerocopy(userp_dec dec, size_t *length_out);


/* Zerocopy API

libuserp offers several decoding functions to facilitate reading data from the
stream without copying it.  Using these functions exposes a bit of the underlying
buffering of the library.  Userp tracks all source data in shared buffers with
reference counting.  It then has another layer of "buffer ranges" objects which
hold references to the shared buffers.  Shared buffers are bound to a single
userp_env object, because userp_env objects define the allocation function used
to resize or free the buffers.

If you dynamically fill your buffers, (such as from a reader callback) then they
have been dynamicallly allocated and need freed.  In order to make sure that
the zerocopy pointers you have don't get freed until you are done with them, you
need to inform the library about which ones you are still using.
Use `userp_buffer_ranges_clone` to create a buffer_ranges object of your own that
has strong references to the underlying buffer objects.  If you do this, the
userp_env will not be freed until all buffer ranges objects you hold have been freed.

However, you can skip most of this complexity if you know for a fact that all the
buffers userp is parsing are under your control.  For example, if you memory-map
a file and then tell userp to wrap that buffer, userp knows it did not allocate
that buffer so it will not free it, and all the pointers returned in the buffer
ranges can safely be used by your program until you unmap the file.  Also, if the
entire stream is found in one buffer like that, you can always assume that there
will be exactly one range returned by any successful Zerocopy API call, making it
safe to write code like

  userp_buffer_ranges ranges;
  if ((ranges= SOME_API_zerocopy(...))) {
	my_data_structure->something= (MySomething*) ranges->range[0].start;
  }

*/
