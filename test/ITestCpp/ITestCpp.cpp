/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include "ITestCpp.h"
#include "ResultWriter.h"
#include "GdbIface.h"

using namespace std;
using namespace ITestCpp;
using namespace ITestCpp::StrUtil;
using std::vector;

bool WaitForParent= true;

/** Class that stores information about test execution until main() is ready to process it and display the result.
 */
struct TestResult {
	bool Passed;
	int DebugPid;
	String DebugCore;
	String Msg;
	TestResult(): Passed(false), DebugPid(0) {}
		
	vector<TestResult> SubTests;
};

/** Wildly overgrown entry point, which performs the entire test sequence.
 * XXX divide this into sub-functions.
 */
int main(int argc, char**argv) {
	int pid;
	int status;

	printf("\n"
	    "Unit Testing System\n"
	    "-------------------\n");
	
	// XXX TODO, implement treed testing.
	TestList &Tests= MasterTestList;
	vector<int> Pids(Tests.Cases.size());
	vector<TestResult> Result(Tests.Cases.size());
	
	if (argc > 1) {
		// run a specific test case, probably for debugging purposes
		const TestCase *Found;
		for (int i= 0; i<Tests.Cases.size(); i++)
			if (strcmp(Tests.Cases[i].GetName(), argv[1]) == 0)
				Found= &Tests.Cases[i];
		if (!Found) {
			printf("Unknown test: %s\n", argv[1]);
			return 1;
		}
		WaitForParent= false; // global flag, affects Fail()
		Found->Run();
		return 0;
	}
	
	// Spawn off the tests
	for (int i= 0; i<Tests.Cases.size(); i++) {
		printf(" * Launching test: %s\n", Tests.Cases[i].GetName());
		switch (pid= fork()) {
		case 0:
			setpgid(0, 0);
			Tests.Cases[i].Run();
			exit(0);
			break;
		case -1:
			perror("fork");
			exit(-1);
			break;
		default:
			Pids[i]= pid;
		}
	}
	
	// Collect the results
	int Remaining= Tests.Cases.size();
	while (Remaining) {
		pid= waitpid(-1, &status, WUNTRACED);
		if (pid == -1) {
			perror("waitpid");
			exit(-1);
		}
		Remaining--;
		int TestIdx= 0;
		for (; TestIdx<Tests.Cases.size(); TestIdx++)
			if (Pids[TestIdx] == pid)
				break;
		
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) == 0) {
				Result[TestIdx].Passed= true;
				Result[TestIdx].Msg= "Success";
			}
			else {
				Result[TestIdx].Passed= false;
				Result[TestIdx].Msg= StrFormat("Exited with code %d", WEXITSTATUS(status));
			}
		}
		else if (WIFSIGNALED(status)) {
			Result[TestIdx].Passed= false;
			Result[TestIdx].Msg= StrFormat("Terminated by signal %d", WTERMSIG(status));
#ifdef WCOREDUMP
			if (WCOREDUMP(status)) {
				Result[TestIdx].Msg+= " and core was dumped";
			}
#endif
		}
		else if (WIFSTOPPED(status)) {
			Result[TestIdx].Passed= false;
			Result[TestIdx].Msg= StrFormat("Was stopped by signal %d", WSTOPSIG(status));
			Result[TestIdx].DebugPid= pid;
		}
		printf(" >> %s: %s\n", Tests.Cases[TestIdx].GetName(), Result[TestIdx].Msg.c_str());
	}
	
	// Write the results as html, using a ResultWriter to ease the pain of generating and escaping html
	printf("Printing Results to TestResult.html\n\n");
	ResultWriter out("TestResult.html");
	out.beginPage().p("  <div class='content'>\n");
	out.p("<h3>Unit Test Results</h3>\n");
	for (int i=0; i<Tests.Cases.size(); i++) {
		out.p("\n<div><span class='testname'>").pText(Tests.Cases[i].GetName()).p("</span>");
		out.p("<span class='").p(Result[i].Passed? "testpass'>Passed":"testfail'>Failed").p("</span>\n");
		if (!Result[i].Passed) {
			out.beginToggle(Tests.Cases[i].GetName(), true);
			out.pExpandBtn(Tests.Cases[i].GetName()).p("Details...<br/>\n");
			out.nextToggle(false).p("\n");
			out.pCollapseBtn(Tests.Cases[i].GetName()).p("Hide<br/>\n");
			//out.beginBox("Stack trace:");
			if (Result[i].DebugPid) {//|| Result[i].DebugCore != "") {
				kill(Result[i].DebugPid, SIGCONT); // wake it up so GDB doesn't hang
				GdbIface *gdb= new GdbIface(Result[i].DebugPid);
				gdb->DoCmd("-exec-interrupt", NULL);
				vector<StackEntry> StackTrace= gdb->GetStackTrace();
				out.p("<table>\n"
					"  <col width='1.5em'></col>" // spacer column that adds a left-margin to the variable lists
					"  <col width='15%'></col>"   // column as wide as variable names
					"  <col width='25%'></col>"   // column which, when added to the prev two, is the width of a testcase name
					"  <col width='50%'></col>"   // column that holds the filename where the testcase originates
					"  <col width='5%'></col>"    // column that holds the line number where the test case originates
					"  <col width='*'></col>\n"   // column which, combined with the prev 3, is the width given for variable values
					"  <tr>\n"
					"    <th class='stack-header' colspan='3'>Function Name</th>\n"
					"    <th class='stack-header'>Source File</th>\n"
					"    <th class='stack-header'>Line</th><th style='padding-left:5em'> </th>\n"
					"  </tr>");
				int Frame= 0;
				for (; Frame < StackTrace.size(); Frame++)
					if (StackTrace[Frame].Func == "ITestCpp::AssertUtil::Fail")
						break;
				for (Frame++; Frame < StackTrace.size(); Frame++)
					if (StackTrace[Frame].Func == "main")
						break;
					else {
						StackFrameDetails FrameDetails= gdb->GetFrameDetails(Frame);
						out.p("<tr>\n"
							"    <td colspan='3' class='stack-func'>").pText(StackTrace[Frame].Func).p("</td>\n"
							"    <td class='stack-srcfile'>").pText(StackTrace[Frame].SrcFile).p("</td>\n"
							"    <td class='stack-srcline'>").pText(StrFormat("%d",StackTrace[Frame].Line)).p("</td><td> </td>\n"
						    "  </tr>\n"
							"  <tr><td style='padding-left:1.5em'> </td><th class='frame-header'>Argument</th><th class='frame-header' colspan='4'>Value</th></tr>\n");
						for (int Arg=0; Arg<FrameDetails.Args.size(); Arg++)
							out.p("  <tr><td> </td><td class='stack-vars'>")
								.pText(FrameDetails.Args[Arg].Name)
								.p("</td><td class='stack-vars' colspan='4'>")
								.pText(FrameDetails.Args[Arg].Value)
								.p("</td></tr>\n");
						out.p("  <tr><td> </td><th class='frame-header'>Local Var</th><th class='frame-header' colspan='4'>Value</th></tr>\n");
						for (int Local=0; Local<FrameDetails.Locals.size(); Local++)
							out.p("  <tr><td> </td><td class='stack-vars'>")
								.pText(FrameDetails.Locals[Local].Name)
								.p("</td><td class='stack-vars' colspan='4'>")
								.pText(FrameDetails.Locals[Local].Value)
								.p("</td></tr>\n");
						out.p("  <tr><td style='padding-bottom:0.5em'> </td></tr>\n");
					}
				out.p("\n</table>");
				gdb->DoConsoleCmd("\"kill\"", NULL);
				delete gdb;
				kill(pid, SIGCONT);
			}
			else {
				out.p("Unavailable.");
			}
			//out.endBox()
			out.endToggle().p("\n");
		}
		out.p("</div>");
	}
	out.p("  </div>\n").endPage();
	out.flush();
}

namespace ITestCpp {
// Namespace for the assertion routines
namespace AssertUtil {

void Fail() {
	if (WaitForParent) {
		kill(getpid(), SIGSTOP); // halt, so that the parent finds out.
	
		volatile bool hang= true;
		while (hang) { // now loop here so that the debugger can attach
			sleep(1);
		}
	}
	fprintf(stderr, "Child %d exiting after Fail\n\n", getpid());
	fflush(stderr);
	exit(0);
}

} // namespace AssertUtil

// This is the namespace for all the testing-quality (i.e. not necessarily
// production quality) string routines that we want to be available in the
// test cases. Its in a namespace so that the test cases aren't forced to use
// it.
namespace StrUtil {
	String StrFormat(const char *FmtStr, ...) {
		char Buffer[128];

		va_list va;
		va_start(va, FmtStr);
		unsigned int CharsUsed= 1+vsnprintf(Buffer, sizeof(Buffer), FmtStr, va);
		va_end(va);

		if (CharsUsed < sizeof(Buffer))
			return String(Buffer, CharsUsed);
		else {
			char* Dest= new char[CharsUsed+1];
			va_start(va, FmtStr);
			CharsUsed= 1+vsnprintf(Dest, CharsUsed+1, FmtStr, va);
			va_end(va);
			String Result(Dest, CharsUsed);
			delete Dest;
			return Result;
		}
	}
} // namespace StrUtil
} // namespace ITestCpp
