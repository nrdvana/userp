#include "local.h"
#include "userp_private.h"

/*
## Userp Diagnostics

### Synopsis

    userp_diag d= userp_env_get_last_error(env);
    if (userp_diag_get_code(d) == USERP_EALLOC)
      ...
    else
      userp_diag_print(d, stderr);

### Description

libuserp reports errors, warnings, and debug information via the userp_diag object.
The library stores useful details into this object, and then (dependong on what
callback you register on the `userp_env`) you receive the object and can make
decisions about what to do based on its attributes, or just format it as text.

### Attributes

#### code

  int code= userp_diag_get_code(diag);
  const char *name= userp_diag_code_name(code);

The primary attribute is 'code'.  This determines which other fields are relevant.
The following codes are defined:

```
```

The following macros let you test the severity of an error code:

```
USERP_IS_FATAL(code)
USERP_IS_ERROR(code)
USERP_IS_WARN(code)
USERP_IS_DEBUG(code)
```

Fatal errors indicate something that even a robust error handler probably can't
recover from.  The default handler will call abort().

Regular errors indicate a problem that prevents whatever action was being attempted,
but if the calling code checks the return value it can fail gracefully.

Warnings indicate potential problems that the caller should be aware of but which
are not interrupting use of the library.

Debug messages are diagnostics to record what is occurring inside the library to
help the developer track down problems.  These are not generated unless the flag is
set in the userp environment.

*/

int userp_diag_get_code(userp_diag diag) {
	return diag->code;
}
const char * userp_diag_code_name(int code) {
	#define RETSTR(x) case x: return #x;
	switch (code) {
	RETSTR(USERP_EFATAL)
	RETSTR(USERP_EBADSTATE)
	RETSTR(USERP_ERROR)
	RETSTR(USERP_EALLOC)
	RETSTR(USERP_EDOINGITWRONG)
	RETSTR(USERP_ETYPESCOPE)
	RETSTR(USERP_ESYS)
	RETSTR(USERP_EPROTOCOL)
	RETSTR(USERP_EFEEDME)
	RETSTR(USERP_ELIMIT)
	RETSTR(USERP_ESYMBOL)
	RETSTR(USERP_ETYPE)
	RETSTR(USERP_WARN)
	RETSTR(USERP_WLARGEMETA)
	default:
		return "unknown";
	}
	#undef RETSTR
}

/*
#### buffer

  char *data;
  size_t pos, len;
  bool success= userp_diag_get_buffer(diag, &buf, &pos, &len);

Most operations in libuserp happen in the context of a shared buffer.  If one was
relevant to this diagnostic, this gives you a reference to it, along with the
current position and length of relevant bytes.

The output parameters are optional (if they are NULL, the value is not copied).
It returns true if a buffer was referenced by this diagnistic.

*

bool userp_diag_get_buffer(userp_diag diag, userp_buffer *buf_p, size_t *pos_p, size_t *len_p) {
	if (!diag->buf)
		return false;
	if (buf_p) *buf_p= diag->buf;
	if (pos_p) *pos_p= diag->pos;
	if (len_p) *len_p= diag->len;
	return true;
}

/*
#### index

Generic attribute for reporting "Nth thing".  Returns 0 if unused.

#### size

Generic attribute reporting total size (bytes) of something.

#### count

Generic attribute reporting total number of things.

*/

int userp_diag_get_index(userp_diag diag) {
	return diag->index;
}
size_t userp_diag_get_size(userp_diag diag) {
	return diag->size;
}
size_t userp_diag_get_count(userp_diag diag) {
	return diag->count;
}

/*
### Methods

#### userp_diag_format

  char buffer[SOME_NUMBER];
  int needed= userp_diag_format(diag, buffer, sizeof(buffer));

Like the snprintf function, this writes data into a buffer up to a maximum allocation size
and then returns the total number of characters (not including NULL) that would have been
written to the buffer if it was infinitely large.

The error message may be translated or modified in any version of the library.  Do not
depend on its output.

#### userp_diag_print

   int wrote= userp_diag_print(diag, stderr);

Stream the diagnostic message into FILE, returning the total number of bytes written,
or ``` -1 - bytes_written ``` if there is an I/O error.

*/

static int userp_process_diag_tpl(userp_diag diag, char *buf, size_t buf_len, FILE *fh);

int userp_diag_format(userp_diag diag, char *buf, size_t buflen) {
	return userp_process_diag_tpl(diag, buf, buflen, NULL);
}

int userp_diag_print(userp_diag diag, FILE *fh) {
	return userp_process_diag_tpl(diag, NULL, 0, fh);
}

int userp_process_diag_tpl(userp_diag diag, char *buf, size_t buf_len, FILE *fh) {
	const char *from, *pos= diag->tpl, *p1, *p2;
	char tmp_buf[sizeof(diag->buffer)*4], id;
	size_t n= 0, str_len= 0;
	bool hex= false, at_pos;
	long long val= 0;
	while (*pos) {
		// here, *pos is either \x01 followed by a code indicating which variable to insert,
		// or any other character means to scan forward.
		if (*pos == '\x01') {
			from= tmp_buf; // source of next block of character data to emit
			n= 0; // number of chars to copy from 'from'
			++pos;
			switch (id= *pos++) {
			// Buffer, displayed as a pointer address
			case USERP_DIAG_PTR_ID:
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%p", diag->ptr);
				break;
			// Buffer, hexdump the contents
			case USERP_DIAG_BUFHEX_ID:
				hex= true;
				n= 0;
				if (0) {
			// Buffer, printed as a string with escapes
			case USERP_DIAG_BUFSTR_ID:
					hex= false;
					tmp_buf[0]= '"';
					n= 1;
				}
				p1= (char*) diag->buffer;
				p2= p1 + diag->len;
				if (p2 > p1 + sizeof(diag->buffer))
					p2= p1 + sizeof(diag->buffer);
				for (; n+9 < sizeof(tmp_buf) && p1 < p2; p1++) {
					at_pos= p1 == (diag->buffer + diag->pos);
					if (hex || *p1 < 32 || *p1 >= 0x7F || *p1 == '\\' || *p1 == '"') {
						if (at_pos) {
							if (hex) {
								tmp_buf[n++]= '>';
							} else {
								tmp_buf[n++]= '"';
								tmp_buf[n++]= ' ';
								tmp_buf[n++]= '>';
								tmp_buf[n++]= '"';
							}
						}
						if (!hex) {
							tmp_buf[n++]= '\\';
							tmp_buf[n++]= 'x';
						}
						tmp_buf[n++]= "0123456789ABCDEF"[(*p1>>4) & 0xF];
						tmp_buf[n++]= "0123456789ABCDEF"[*p1 & 0xF];
						if (hex) {
							tmp_buf[n++]= ' ';
						}
					}
					else tmp_buf[n++]= *p1;
				}
				if (hex) n--;
				else tmp_buf[n++]= '"';
				tmp_buf[n]= '\0';
				break;
			// integer with "power of 2" notation
			case USERP_DIAG_ALIGN_ID:
				val= diag->align; if (0)
				n= snprintf(tmp_buf, sizeof(tmp_buf), "2**%lld", val);
				break;
			// Generic integer fields
			case USERP_DIAG_INDEX_ID:
				val= diag->index; if (0)
			case USERP_DIAG_POS_ID:
				val= diag->pos; if (0)
			case USERP_DIAG_POS2_ID:
				val= diag->pos2; if (0)
			case USERP_DIAG_LEN_ID:
				val= diag->len; if (0)
			case USERP_DIAG_LEN2_ID:
				val= diag->len2; if (0)
			case USERP_DIAG_SIZE_ID:
				val= diag->size; if (0)
			case USERP_DIAG_SIZE2_ID:
				val= diag->size2; if (0)
			case USERP_DIAG_COUNT_ID:
				val= diag->count; if (0)
			case USERP_DIAG_COUNT2_ID:
				val= diag->count2;
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%lld", val);
				break;
			default:
				// If we get here, it's a bug.  If the second byte was NUL, back up one, else substitute an error message
				if (!pos[-1]) --pos;
				from= "(unknown var)";
				if (0)
			case USERP_DIAG_CSTR1_ID:
				from= diag->cstr1; if (0)
			case USERP_DIAG_CSTR2_ID:
				from= diag->cstr2;
				if (!from) from= "(NULL)";
				n= strlen(from);
			}
		}
		else {
			from= pos;
			while (*pos > 1) ++pos;
			n= pos-from;
		}
		if (n) {
			if (!from) {
				from= "(NULL)";
				n= 6;
			}
			if (buf && str_len+1 < buf_len) // don't call memcpy unless there is room for at least 1 char and NUL
				memcpy(buf+str_len, from, (str_len+n+1 <= buf_len)? n : buf_len-1-str_len);
			if (fh) {
				if (!fwrite(from, 1, n, fh))
					return -1 - str_len;
			}
			str_len += n;
		}
	}
	// at end, NUL-terminate the buffer, if provided and has a length
	if (buf && buf_len > 0)
		buf[str_len+1 > buf_len? buf_len-1 : str_len]= '\0';
	return str_len;
}

void userp_diag_set(userp_diag diag, int code, const char *tpl) {
	diag->code= code;
	diag->tpl= tpl;
}

void userp_diag_setf(userp_diag diag, int code, const char *tpl, ...) {
	const char *from;
	size_t skip;
	va_list ap;
	va_start(ap, tpl);
	diag->code= code;
	diag->tpl= tpl;
	while (*tpl) {
		if (*tpl++ == '\x01') {
			switch (*tpl++) {
			case USERP_DIAG_BUFHEX_ID:
			case USERP_DIAG_BUFSTR_ID:
				from= va_arg(ap, char*);
				diag->pos= va_arg(ap, size_t);
				diag->len= va_arg(ap, size_t);
				// copy data into buffer, making sure to include 'pos' if the data is
				// too large
				if (diag->pos > sizeof(diag->buffer)*9/10) {
					skip= diag->pos - sizeof(diag->buffer)*9/10;
					from += skip;
					diag->pos -= skip;
					diag->len -= skip;
				}
				if (diag->len > sizeof(diag->buffer))
					diag->len= sizeof(diag->buffer);
				memcpy(diag->buffer, from, diag->len);
				break;
			case USERP_DIAG_ALIGN_ID:
				diag->align= va_arg(ap, int);
				break;
			case USERP_DIAG_INDEX_ID:
				diag->index= va_arg(ap, int);
				break;
			case USERP_DIAG_POS_ID:
				diag->pos= va_arg(ap, size_t);
				break;
			case USERP_DIAG_POS2_ID:
				diag->pos2= va_arg(ap, size_t);
				break;
			case USERP_DIAG_LEN_ID:
				diag->len= va_arg(ap, size_t);
				break;
			case USERP_DIAG_LEN2_ID:
				diag->len2= va_arg(ap, size_t);
				break;
			case USERP_DIAG_SIZE_ID:
				diag->size= va_arg(ap, size_t);
				break;
			case USERP_DIAG_SIZE2_ID:
				diag->size2= va_arg(ap, size_t);
				break;
			case USERP_DIAG_COUNT_ID:
				diag->count= va_arg(ap, size_t);
				break;
			case USERP_DIAG_COUNT2_ID:
				diag->count2= va_arg(ap, size_t);
				break;
			case USERP_DIAG_CSTR1_ID:
				diag->cstr1= va_arg(ap, const char *);
				break;
			case USERP_DIAG_CSTR2_ID:
				diag->cstr2= va_arg(ap, const char *);
				break;
			case USERP_DIAG_PTR_ID:
				diag->ptr= va_arg(ap, void *);
				break;
			default:
				fprintf(stderr, "BUG: Unhandled diagnostic variable %d", *tpl);
				abort();
			}
		}
	}
	va_end(ap);
}


#ifdef UNIT_TEST

UNIT_TEST(diag_simple_string) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));

	printf("# simple string\n");
	userp_diag_set(&d, 1, "Simple string");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# empty string\n");
	userp_diag_set(&d, 1, "");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
# simple string
Simple string
wrote=13
# empty string

wrote=0
*/

UNIT_TEST(diag_tpl_ref_static_string) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));

	printf("# cstr1 in middle\n");
	userp_diag_setf(&d, 1, "String ref '" USERP_DIAG_CSTR1 "'", "TEST");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# cstr1 at end\n");
	userp_diag_setf(&d, 1, "Ends with " USERP_DIAG_CSTR1, "TEST");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# cstr1 at start\n");
	userp_diag_setf(&d, 1, USERP_DIAG_CSTR1 " and things", "TEST");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# cstr1 is entire string\n");
	userp_diag_setf(&d, 1, USERP_DIAG_CSTR1, "TEST");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# cstr1 is empty\n");
	userp_diag_setf(&d, 1, "'" USERP_DIAG_CSTR1 "'", "");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# cstr1 and cstr2\n");
	userp_diag_setf(&d, 1, "String ref '" USERP_DIAG_CSTR2 "', and '" USERP_DIAG_CSTR1 "'", "2", "1");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	printf("# empty cstr1 and cstr2\n");
	userp_diag_setf(&d, 1, USERP_DIAG_CSTR2 USERP_DIAG_CSTR1, "", "");
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
# cstr1 in middle
String ref 'TEST'
wrote=17
# cstr1 at end
Ends with TEST
wrote=14
# cstr1 at start
TEST and things
wrote=15
# cstr1 is entire string
TEST
wrote=4
# cstr1 is empty
''
wrote=2
# cstr1 and cstr2
String ref '2', and '1'
wrote=23
# empty cstr1 and cstr2

wrote=0
*/

UNIT_TEST(diag_ref_buf_hex) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));

	userp_diag_setf(&d, 1, "Some Hex: " USERP_DIAG_BUFHEX,
		"\x01\x02\x03\x04", 1, 3);
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
Some Hex: 01 >02 03
wrote=19
*/

UNIT_TEST(diag_ref_bufstr) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));

	userp_diag_setf(&d, 1, "String: " USERP_DIAG_BUFSTR,
		"test\0\x01\x02", 7, 7);
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
String: "test\\x00\\x01\\x02"
wrote=26
*/

#endif
