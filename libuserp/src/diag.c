#include "local.h"
#include "userp_private.h"

/*
## userp_diag

libuserp reports errors, warnings, and debug information via the userp_diag object.
The library stores useful details into this object, and then (dependong on what
callback you register on the `userp_env`) you receive the object and can decide
whether to format it as text, or operate on it as data.

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
	RETSTR(USERP_EALLOC)
	RETSTR(USERP_EINVAL)
	RETSTR(USERP_EEOF)
	RETSTR(USERP_ELIMIT)
	RETSTR(USERP_WLARGEMETA)
	default:
		return "unknown";
	}
	#undef RETSTR
}

/*
#### buffer

  userp_buffer buf;
  size_t pos, len;
  bool success= userp_diag_get_buffer(diag, &buf, &pos, &len);

Most operations in libuserp happen in the context of a shared buffer.  If one was
relevant to this diagnostic, this gives you a reference to it, along with the
current position and length of relevant bytes.

The output parameters are optional (if they are NULL, the value is not copied).
It returns true if a buffer was referenced by this diagnistic.

*/

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
	char tmp_buf[64], id;
	size_t n= 0, tmp_len, str_len= 0;
	int i;
	while (*pos) {
		// here, *pos is either \x01 followed by a code indicating which variable to insert,
		// or any other character means to scan forward.
		if (*pos == '\x01') {
			n= 0;
			from= tmp_buf;
			++pos;
			switch (id= *pos++) {
			// Buffer, displayed as a pointer address
			case USERP_DIAG_BUFADDR_ID:
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%p", diag->buf->data + diag->pos);
				break;
			// Buffer, hexdump the contents
			case USERP_DIAG_BUFHEX_ID:
				p1= diag->buf->data + diag->pos;
				p2= p1 + diag->len;
				for (n= 0; (n+1)*3 < sizeof(tmp_buf) && p1 < p2; p1++) {
					tmp_buf[n++]= "0123456789ABCDEF"[(*p1>>4) & 0xF];
					tmp_buf[n++]= "0123456789ABCDEF"[*p1 & 0xF];
					tmp_buf[n++]= ' ';
				}
				if (n) --n;
				tmp_buf[n]= '\0';
				break;
			// integer with "power of 2" notation
			case USERP_DIAG_ALIGN_ID:
				n= snprintf(tmp_buf, sizeof(tmp_buf), "2**%d", diag->align);
				break;
			// Generic integer fields
			case USERP_DIAG_POS_ID:
				n= diag->pos; if (0)
			case USERP_DIAG_LEN_ID:
				n= diag->len; if (0)
			case USERP_DIAG_SIZE_ID:
				n= diag->size; if (0)
			case USERP_DIAG_COUNT_ID:
				n= diag->count;
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%ld", n);
				break;
			default:
				// If we get here, it's a bug.  If the second byte was NUL, back up one, else substitute an error message
				if (!pos[-1]) --pos;
				from= "(unknown var)";
				if (0)
			// Buffer, printed as a string
			case USERP_DIAG_BUFSTR_ID:
				from= diag->buf->data + diag->pos;
				n= diag->len;
				break;
			case USERP_DIAG_CSTR1_ID:
				from= diag->cstr1; if (0)
			case USERP_DIAG_CSTR2_ID:
				from= diag->cstr2;
				n= strlen(from);
			}
		}
		else {
			from= pos;
			while (*pos > 1) ++pos;
			n= pos-from;
		}
		if (n) {
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

void userp_diag_set(userp_diag diag, int code, const char *tpl, userp_buffer buf) {
	diag->code= code;
	diag->tpl= tpl;
	//if (diag->buf) userp_drop_buffer(diag->buf);
	diag->buf= buf;
	//if (buf) userp_grab_buffer(diag->buf);
}


#ifdef WITH_UNIT_TESTS

UNIT_TEST(simple_string) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));

	userp_diag_set(&d, 1, "Simple string", NULL);
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	userp_diag_set(&d, 1, "", NULL);
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
Simple string
wrote=13

wrote=0
*/

UNIT_TEST(tpl_ref_static_string) {
	int wrote;
	struct userp_diag d;
	bzero(&d, sizeof(d));
	userp_diag_set(&d, 1, "String ref '" USERP_DIAG_CSTR1 "'", NULL);
	d.cstr1= "TEST";
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);

	userp_diag_set(&d, 1, "String ref '" USERP_DIAG_CSTR2 "', and '" USERP_DIAG_CSTR1 "'", NULL);
	d.cstr1= "1";
	d.cstr2= "2";
	wrote= userp_diag_print(&d, stdout);
	printf("\nwrote=%d\n", wrote);
}
/*OUTPUT
String ref 'TEST'
wrote=17
String ref '2', and '1'
wrote=23
*/

#endif
