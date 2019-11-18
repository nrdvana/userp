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

=head1 SPECIFICATION

A symbol type is either the set of all symbols, a list of specific symbols, or a subset of
symbols by prefix (namespace).  The namespace doesn't actually have any effect on the encoding,
but can make the API more convenient and help with integrating with the host language.

  (values=(EPERM ENOENT ESRCH EINTR))

  (prefix=POSIX.libc. values=(EPERM ENOENT ESRCH EINTR))

=cut

has values => ( is => 'ro' );
has prefix => ( is => 'ro' );

sub isa_sym { 1 }

1;
