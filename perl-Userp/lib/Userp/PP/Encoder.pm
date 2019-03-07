package Userp::PP::Encoder;

=head1 DESCRIPTION

This class is the API for serializing data into the types defined in the current scope.

The sequence of API calls depends on type of the value being encoded.  Here is a brief
overview:

=over

=item Integer

    $enc->encode_int($int_type, $value);

=item Enum

    $enc->encode_enum_ident ($enum_type, $ident);      # string identifier or Ident object
    $enc->encode_enum_member($enum_type, $index);      # index of selected member value
    $enc->encode_enum_value ($enum_type, $int_value);  # integer value to be encoded

=item Union

    # When a union type is already implied, just call encode for one of its member types
    # The union member will be selected automatically.
    $enc->encode_Something($some_type, @args);
    
    # When the current type is "Any" or ambiguous member of some union, select the union
    # type and then continue calling methods to select a sub-type, then encode a value.
    $enc->select_union($union_type)->...

=item Sequence

    $enc->begin_seq($seq_type, $length); # length may or may not be required
    for(...) {
      $enc->seq_elem_ident($ident); # needed if elements are named and ident is not pre-defined
      $enc->encode_Something(...)
    }
    $enc->end_seq();
    
    # Special case for byte/integer arrays or strings or anything compatible:
    $enc->encode_array($seq_type, \@values);
    
    # Special case for named element sequences
    $enc->encode_record($seq_type, \%key_val);

=item Ident

    $enc->encode_ident($ident_type, $ident_value);

=back

=cut
