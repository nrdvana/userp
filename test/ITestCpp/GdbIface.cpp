/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#include <stdio.h>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <stdlib.h>
#include <string>
//#include <pcre.h>
#include "GdbIface.h"
#include "ITestCpp.h"

namespace ITestCpp {
	
using ITestCpp::StrUtil::StrFormat;
using std::string;
using std::vector;
using std::map;

/*
pcre* RegexCompileOrDie(const char* pattern, int options) {
	const char *error;
	int at;
	pcre* result= pcre_compile(pattern, options, &error, &at, NULL);
	if (!result) {
		fprintf(stderr, "Regex compile failed: %s\n%s\n%*s\n\n", error, pattern, at+1, "^");
		exit(1);
	}
	return result;
}

//const pcre * const StackEntryRegex= RegexCompileOrDie("level=\"(.[^\"])\"", 0);
*/

//----------------------------------------------------------------------------
// Class GdbIface
//----------------------------------------------------------------------------

/** Gdb constructor.
 * This sets up pipes, forks off a child process, and then the child becomes gdb.
 * There isn't a whole lot of error checking, but the order the functions are
 * called pretty much guarantees correct behavior.
 *
 * This function does not read the initial text sent by gdb because it could
 * unexpectedly hang the caller.  Also, this gives the caller a chance to do
 * things like SIGCONT the target so that gdb can attach to it.  The initial
 * read will be performed the first time the caller invokes a public method.
 */
GdbIface::GdbIface(int TargetPid, bool Debug): _debug(Debug) {
	// Make two pipes, us->gdb and gdb->us
	int fds[4];
	if (pipe(fds) != 0 || pipe(fds+2) != 0) {
		perror("pipe");
		exit(0);
	}
	// Fork off a gdb instance
	switch (GdbPID= fork()) {
	case 0: // Child
		close(fds[1]);
		close(fds[2]);
		// stdin
		close(0);
		dup(fds[0]);
		close(fds[0]);
		// stdout
		close(1);
		dup(fds[3]);
		close(fds[3]);
		RunGDB(TargetPid);
	case -1: // Fork failed
		perror("fork");
		exit(-1);
	default: // Parent
		close(fds[0]);
		close(fds[3]);
		RecvPipe= fdopen(fds[2], "r");
		SendPipe= fdopen(fds[1], "w");
		_HaveReadInit= false;
	}
}

/** Run gdb.
 * Only reason this wa ssplit off into a function was for code clarity.
 */
void GdbIface::RunGDB(int TargetPid) {
//	char Buffer[1000];
//	sprintf(Buffer, "tee togdb | gdb -nx --interpreter=mi2 unittests %d | tee fromgdb", TargetPid);
//	execlp("/bin/sh", "sh", "-c", Buffer, (char*) NULL);
	char PidTextBuffer[30];
	const char *GdbParams[]= {"gdb", "-nx", "--interpreter=mi2", "unittests", PidTextBuffer, NULL};
	snprintf(PidTextBuffer, sizeof(PidTextBuffer), "%d", TargetPid);
	if (_debug) {
		fprintf(stderr, "Invoking gdb as:");
		for (const char** p= GdbParams; *p; p++)
			fprintf(stderr, "%s", *p);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	execvp(GdbParams[0], const_cast<char*const*>(GdbParams)); // why the heck do they want char*const*??
	// if we get here, something went wrong
	perror("execvp");
	exit(-1);
}

/** Perform the initial read from gdb.
 * Gdb generates an initial message group which needs to be read before we try
 * reading the results of commands.  We also read the version number.  Since
 * the version number doesn't sound like its guaranteed to be in the initial
 * messages, I specifically request it in a second message group.
 */
void GdbIface::ReadInit() {
	_HaveReadInit= true;
	ReadResult(NULL);
	TTextLines Result;
	DoCmd("-gdb-version", &Result);
	string::size_type pos= Result[0].find("GNU gdb ");
	if (pos != string::npos)
		_version= Result[0].substr(8);
}

/** Kill gdb, and close pipes.
 * Instead of "kill()" I just close the pipes and let gdb find that stdin is
 * closed, and exit normally.  Then I wait for it.  This could hang if gdb isn't
 * responding... might be a good idea to add a timeout or something.
 */
GdbIface::~GdbIface() {
	fclose(SendPipe);
	fclose(RecvPipe);
	int status;
	if (waitpid(GdbPID, &status, 0) == -1)
		perror("waitpid(<gdb>)");
}

/** Read a batch of results ending in "(gdb)".
 * This function collects a list of strings returned from gdb for whatever
 * command was issued.  It concatenates partial reads to preserve the lines
 * as gdb intended them.
 */
void GdbIface::ReadResult(TTextLines *Result) {
	if (Result)
		Result->reserve(Result->size()+20);
	char Buffer[256];
	string Line("");
	if (_debug) {
		fprintf(stderr, " GdbIface: reading...\n");
		fflush(stderr);
	}
	do {
		if (!fgets(Buffer, sizeof(Buffer), RecvPipe))
			throw GdbCommError("Nothing read from pipe");
		Line+= string(Buffer);
		if (Line.substr(0, 5) == "(gdb)")
			break;
		if (Line[Line.size()-1] == '\n') {
			if (Result)
				Result->push_back(Line.substr(0, Line.size()-1));
			if (_debug) {
				fprintf(stderr, " GdbIface: << %s\n", Line.substr(0, Line.size()-1).c_str());
				fflush(stderr);
			}
			Line.clear();
		}
	} while (true);
	if (_debug) {
		fprintf(stdout, " GdbIface: read complete\n");
		fflush(stdout);
	}
}

/** Run a command, and return the response as an array of MI2 results.
 * The terminating "(gdb)" is not included in the array.
 */
void GdbIface::DoCmd(string Cmd, TTextLines* Result) {
	if (!_HaveReadInit)
		ReadInit();
	if (_debug) {
		fprintf(stderr, " GdbIface: >> %s\n", Cmd.c_str());
		fflush(stderr);
	}
	fprintf(SendPipe, "%s\n", Cmd.c_str());
	fflush(SendPipe);
	ReadResult(Result);
}

/** Run a gdb concole command, returning response in Mi2 format.
 */
void GdbIface::DoConsoleCmd(string Cmd, TTextLines *Result) {
	if (!_HaveReadInit)
		ReadInit();
	if (_debug) {
		fprintf(stderr, " GdbIface: >> -interpreter-exec console \"%s\"\n", Cmd.c_str());
		fflush(stderr);
	}
	fprintf(SendPipe, "-interpreter-exec console \"%s\"\n", Cmd.c_str());
	fflush(SendPipe);
	ReadResult(Result);
}

/** Get a stack trace.
 * The stack trace is returned as an array of StackEntry.  Each StackEntry has
 * its fields populated from whatever information gdb had available for that
 * frame, and so some fields may be left blank.
 */
vector<StackEntry> GdbIface::GetStackTrace() {
	if (!_HaveReadInit)
		ReadInit();

	TTextLines Result;
	DoCmd("-stack-list-frames", &Result);
	for (int i=0; i<Result.size(); i++)
		if (Result[i].size() > 12 && Result[i].substr(0,12) == "^done,stack=") {
			vector<StackEntry> Frames;
			Frames.reserve(20);
			GdbTupleIterator FrameIter(Result[i].substr(12));
			while (FrameIter.Next()) {
				Frames.push_back(StackEntry());
				GdbTupleIterator FramePropsIter(FrameIter.ElemValue());
				while (FramePropsIter.Next()) {
					string name= FramePropsIter.ElemName();
					if (name == "func")
						Frames.back().Func= FramePropsIter.ElemValue();
					else if (name == "file")
						Frames.back().SrcFile= FramePropsIter.ElemValue();
					else if (name == "line")
						Frames.back().Line= atoi(FramePropsIter.ElemValue().c_str());
				}
			}
			return Frames;
		}
	throw GdbCommError("Didn't get result of stack trace from gdb");
}

void ReadNameVal(NameValPair &Result, string tuple) {
	GdbTupleIterator NameAndVal(tuple);
	while (NameAndVal.Next()) {
		string name= NameAndVal.ElemName();
		if (name == "name")
			Result.Name= NameAndVal.ElemValue();
		else if (name == "value")
			Result.Value= NameAndVal.ElemValue();
	}
}

void ReadArgs(StackFrameDetails &Result, string FrameVal) {
	GdbTupleIterator FramePropsIter(FrameVal);
	while (FramePropsIter.Next()) { // search for "args="
		if (FramePropsIter.ElemName() == "args") {
			GdbTupleIterator ArgsIter(FramePropsIter.ElemValue());
			while (ArgsIter.Next()) { // for each function arg...
				Result.Args.push_back(NameValPair());
				ReadNameVal(Result.Args.back(), ArgsIter.ElemValue());
			}
			return;
		}
	}
	throw GdbParseError("Didn't find the args values in the result");
}

void ReadLocals(StackFrameDetails &Result, string LocalsListVal) {
	GdbTupleIterator LocalsIter(LocalsListVal);
	while (LocalsIter.Next()) {
		Result.Locals.push_back(NameValPair());
		ReadNameVal(Result.Locals.back(), LocalsIter.ElemValue());
	}
}

/** Get the details for a stack frame.
 * Currently, this consists of locals and arguments.
 */
StackFrameDetails GdbIface::GetFrameDetails(int FrameNum) {
	StackFrameDetails Result;
	TTextLines Response;
	DoCmd(StrFormat("-stack-list-arguments 1 %d %d", FrameNum, FrameNum), &Response);
	bool done= false;
	for (int i=0; !done && i<Response.size(); i++)
		if (Response[i].size() > 17 && Response[i].substr(0,17) == "^done,stack-args=") {
			GdbTupleIterator FrameIter(Response[i].substr(17));
			if (!FrameIter.Next()) // one frame should have been returned
				throw GdbParseError("Didn't find the frame");
			ReadArgs(Result, FrameIter.ElemValue());
			done= true;
		}
	if (!done)
		throw GdbCommError("Didn't get 'stack-args' response from gdb");
	
	DoCmd(StrFormat("-stack-select-frame %d", FrameNum), NULL);
	DoCmd("-stack-list-locals 1", &Response);
	done= false;
	for (int i=0; !done && i<Response.size(); i++)
		if (Response[i].size() > 13 && Response[i].substr(0,13) == "^done,locals=") {
			ReadLocals(Result, Response[i].substr(13));
			done= true;
		}
	if (!done)
		throw GdbCommError("Didn't get 'locals' response form gdb");
	
	return Result;
}

//----------------------------------------------------------------------------
// Gdb response parsing utility functions
//----------------------------------------------------------------------------

string::iterator SeekToEndOfValue(string::iterator Pos, string::iterator Limit);
string::iterator SeekToEndOfTuple(string::iterator Pos, string::iterator Limit);
string::iterator SeekToEndOfScalar(string::iterator Pos, string::iterator Limit);

/** Starting from the first char of a value, find the character position after it.
 * The values recognized are arrays, records, and scalars (which are all quoted)
 */
string::iterator SeekToEndOfValue(string::iterator Pos, string::iterator Limit) {
	if (Pos >= Limit)
		return Limit;
	switch (*Pos) {
	case '[':
	case '{':
 		return SeekToEndOfTuple(Pos, Limit);
	case '"':
		return SeekToEndOfScalar(Pos, Limit);
	default:
		throw GdbParseError(string("SeekToEndOfValue: Error parsing data at: ").append(string(Pos,Limit)));
	}
}

/** Starting from the '"' of a scalar, find the char after the ending '"'.
 * This routine uses a very simple algorithm to skip accross escaped
 * doublequotes and stops when it finds a non-escaped doublequote.
 */
string::iterator SeekToEndOfScalar(string::iterator Pos, string::iterator Limit) {
	if (*Pos != '"')
		throw GdbParseError(string("SeekToEndOfScalar: Error parsing data at: ").append(string(Pos,Limit)));

	++Pos;
	bool escape= false;
	while (Pos < Limit) {
		if (*Pos == '"' && !escape)
			return ++Pos; // "end" is one-past-doublequote
		if (*Pos == '\\') escape= !escape;
		else escape= false;
		++Pos;
	}
	return Limit;
}

/** Get the char that ends whatever the specified char starts.
 * Specifically, this is for determining whether we are matching a '[' or a '('.
 * If the char is not recognized, returns '\0'.
 */
char GetEndChar(char ch) {
	switch (ch) {
	case '[': return ']';
	case '{': return '}';
	default:
		return '\0';
	}
}

/** From the leading puntuation of a tuple, find the char after the closing symbol.
 * This simply seeks accross all comma separated values and stops when it finds
 * the terminating brak/paren.
 *
 * I was a bit suprised to find that gdb emits both records AND arrays with
 * field names... perhaps '[' doesn't really indicate "array".
 */
string::iterator SeekToEndOfTuple(string::iterator Pos, string::iterator Limit) {
	char endchar= GetEndChar(*Pos);
	if (!endchar)
		throw GdbParseError(string("Not at the start of a tuple: ").append(string(Pos, Limit)));
	++Pos;
	while (Pos < Limit) {
		if (*Pos == ',')
			++Pos;
		else if (*Pos == endchar)
			return ++Pos;
		else {
			// Are we reading tuple elements or array elements?
			if (*Pos != '[' && *Pos != '{' && *Pos != '"') {
				// skip the field name
				while (Pos < Limit && *Pos != '=')
					++Pos;
				++Pos;
			}
			Pos= SeekToEndOfValue(Pos, Limit);
		}
	}
	return Limit;
}

/**
 * Yeah I know this can be done with Integer.parseInt.
 *  ...just fun to be ultra-efficient sometimes ;-)
 * (from Michael Conrad's Libraray of Fun Stuff)
 *
 * Look Ma!  No branches!!
 *
 * No error checking is performed.  Characters are assumed to be hex digits.
 *
 * @param high - MSB hex character.  Valid for 0-9,A-F,a-f.  Undefined results otherwise.
 * @param low - LSB hex char.  Valid for 0-9,A-F,a-f.  Undefined results otherwise.
 * @return the value that the hex characters represent
 */
unsigned char hexToByte(char high, char low) {
	int hswitch= high >> 6;
	int lswitch= low >> 6;
	int h= high + hswitch + (hswitch<<3);
	int l= low + lswitch + (lswitch<<3);
	return ((h<<4)&0xF0) | (l&0xF);
}

/** Unquote a string value.
 * This simply follows the C language rules for unquoting a string.
 */
string Unquote(string::iterator Pos, string::iterator Limit) {
	++Pos;
	
	// First, check if we need to do any escaping.  If not, we save some time.
	string temp;
	string::iterator i= Pos, End;
	int chars= 0;
	int EscapeCount= 0;
	bool EscapeNext= false;
	while (++i < Limit) {
		if (EscapeNext)
			EscapeNext= false;
		else {
			chars++;
			if (*i == '"')
				break;
			if (*i == '\\') {
				EscapeCount++;
				EscapeNext= true;
			}
		}
	}
	End= i;

	// If we need to do escaping, build a new string in 'temp' and then set the iterators to it.
	if (EscapeCount) {
		temp.reserve(chars);
		for (i=Pos; i<Limit && *i != '"'; i++)
			if (*i != '\\')
				temp+= *i;
			else if (++i < Limit) {
				switch (*i) {
				case '0': temp+= '\0'; break;
				case 'a': temp+= '\a'; break;
				case 'b': temp+= '\b'; break;
				case 'n': temp+= '\n'; break;
				case 'r': temp+= '\r'; break;
				case 't': temp+= '\t'; break;
				case 'x': {
					if (++i >= Limit) continue;
					int HexH= *i;
					if (++i >= Limit) continue;
					int HexL= *i;
					temp+= hexToByte((char) HexH, (char) HexL);
				}
				default:
					temp+= *i;
				}
			}
		Pos= temp.begin();
		End= temp.end();
	}

	// Return the string from one iterator to the other
	return string(Pos, End);
}

//----------------------------------------------------------------------------
// Class GdbTupleIterator
//----------------------------------------------------------------------------

/** Initialize the fields such that the first call to Next() will set us up for the first element.
 * It might have been more sensible to initialize the first element here, but
 * then the user doesn't get to find out whether there is a first element at all,
 * and they would also deprived of the handy construct
 *
 *   while (GTIter.Next())
 *      process(GTIter.ElemName(), GTIter.ElemValue());
 */
GdbTupleIterator::GdbTupleIterator(string value) {
	_Source= value;
	_ElemIdx= -1;
	_ElemStart= _ElemEnd= _ElemNameEnd= _Source.begin();
	endchar= GetEndChar(*_ElemStart);
	if (!endchar)
		throw GdbParseError(string("Not at the start of a tuple: ").append(_Source));
	_ElemEnd++; // so that we start on the second char, i.e. first data char
}

/** Set up the object for the next element, if one exists.
 * If there are no more elements, this returns false.
 * If there is no selected element, properties will probably segfault if you access them.
 * Else it will return true, and all properties will be valid.
 */
bool GdbTupleIterator::Next() {
	_ElemStart= _ElemEnd;
	// Any more elements?
	if (_ElemStart >= _Source.end() || *_ElemStart == endchar) {
		_ElemNameEnd= _ElemStart;
		return false;
	}
	_ElemIdx++;
	if (*_ElemStart == ',')
		++_ElemStart;
	_ElemNameEnd= _ElemStart;
	// Are we reading tuple elements or array elements?
	if (*_ElemStart == '[' || *_ElemStart == '{' || *_ElemStart == '"') {
		// array.  no names.
		_ElemEnd= _ElemStart;
	}
	else {
		// Record.  skip til the '=' to get the name
		while (_ElemNameEnd < _Source.end() && *_ElemNameEnd != '=')
			++_ElemNameEnd;
		_ElemEnd= _ElemNameEnd;
		++_ElemEnd;
	}
	_ElemEnd= SeekToEndOfValue(_ElemEnd, _Source.end());
	return true;
}

/** Get the name of this element, or an empty string if it has no name.
 * Elements have names only if they are a member of a record.
 */
string GdbTupleIterator::ElemName() {
	return string(_ElemStart, _ElemNameEnd);
}

/** Get the value portion of the current element.
 * If the value is a scalar, it will be stripped of doublequotes.  Else, it
 * will start with a '[' or '(' and indicates a tuple.
 *
 * A better approach would be to have a "type" field and automatically convert
 * the data to something useful, but that would be a lot of work.  Also more
 * code for the user.  And I don't see this class getting very widespread use...
 */
string GdbTupleIterator::ElemValue() {
	string::iterator ValStart= _ElemNameEnd;
	if (*ValStart == '=')
		++ValStart;
	if (*ValStart == '"')
		return Unquote(ValStart, _ElemEnd);
	else
		return string(ValStart, _ElemEnd);
}

}
