package Userp::Type::Symbol;
use Moo;
extends 'Userp::Type';

=head1 DESCRIPTION

The Symbol type represents an infinite printable subset of Unicode strings.  Symbols are not
general-purpose strings; they are intended to represent distinct concepts and are only described
by the unicode strings.  A symbol *should* be composed only of printable non-whitespace
characters, but to keep the implementation simple only the low-ascii control and whitespace
codepoints are forbidden.  Another concession to simplicity is that symbols are only considered
equal if their encoded bytes (UTF-8) are identical.

You can declare types that are an enumeration of symbols.  These differ from an Integer type
with named values in that the symbols have no implied numeric value visible through the API
(though of course they are encoded as integers)

=cut

sub isa_Symbol { 1 }

1;
