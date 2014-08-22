/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#ifndef GDB_IFACE_H
#define GDB_IFACE_H

#include <stdio.h>
#include <vector>
#include <map>
#include <string>

namespace ITestCpp {

struct StackEntry {
	std::string Func;
	std::string SrcFile;
	int Line;
};

struct NameValPair {
	std::string Name;
	std::string Value;
	
	NameValPair() {}
	NameValPair(std::string name, std::string val): Name(name), Value(val) {}
};

// help reduce compiler gibberish in error messages
class TTextLines: public std::vector<std::string> {};

struct StackFrameDetails {
	std::vector<NameValPair> Args;
	std::vector<NameValPair> Locals;
};

/**
 * GdbIface provides debugging and inspection of a process, using gdb.
 */
class GdbIface {
	int GdbPID;
	std::string _version;
	FILE *SendPipe, *RecvPipe;
	bool _HaveReadInit;
	bool _debug;

protected:
	void ReadResult(TTextLines *Result);
	void ReadInit();
	void RunGDB(int TargetPid);

public:
	GdbIface(int TargetPid, bool Debug= false);
	~GdbIface();
	inline std::string Version() { return _version; }
	void DoCmd(std::string Cmd, TTextLines *Result);
	void DoConsoleCmd(std::string Cmd, TTextLines *Result);
	std::vector<StackEntry> GetStackTrace();
	StackFrameDetails GetFrameDetails(int FrameNum);
};

/**
 * GdbError is the base class for all errors gdb-related.
 */
class GdbError {
	std::string Msg;
public:
	GdbError(std::string msg): Msg(msg) {}
	std::string toString() { return Msg; }
};

/**
 * GdbCommError indicates communication errors with the gdb process.
 */
class GdbCommError: public GdbError {
public:
	GdbCommError(std::string Msg): GdbError(Msg) {}
};

/**
 * GdbParseError is used to indicate failure to parse gdb data or messages.
 */
class GdbParseError: public GdbError {
public:
	GdbParseError(std::string Msg): GdbError(Msg) {}
};

/**
 * GdbTupleIterator iterates through the values of a tuple-value returned by gdb's MI2 interface.
 */
class GdbTupleIterator {
	std::string _Source;
	char endchar;
	int _ElemIdx;
	std::string::iterator _ElemStart, _ElemNameEnd, _ElemEnd;
public:
	GdbTupleIterator(std::string value);
	bool Next();
	std::string ElemName();
	std::string ElemValue();
	int ElemIndex() { return _ElemIdx; }
};

} // end namespace

#endif
