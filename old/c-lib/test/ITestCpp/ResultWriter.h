/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include <stdio.h>
#include <stack>
#include "ITestCpp.h"

namespace ITestCpp {

using namespace StrUtil;

/** Simple class that aids in generating useful html structures.
 * This is a c++ version of very similar code in class HtmlGL in Mike Conrad's
 * Brainstormer webapp.
 */
class ResultWriter {
	FILE *f;
	
	struct ToggleEntry {
		const String name;
		int count;
		ToggleEntry(const String &Name, int Count): name(Name), count(Count) {};
	};
	std::stack<ToggleEntry> ToggleStack; // keeps track of nested "toggleable" html divs
	
public:
	ResultWriter(const char *fname);
	~ResultWriter();
	ResultWriter& beginPage();
	ResultWriter& endPage();
	ResultWriter& p(const char* html);
	ResultWriter& pText(const char* text);
	ResultWriter& beginBox(const char* caption= "");
	ResultWriter& endBox();
	ResultWriter& beginToggle(const String &name, bool isActive);
	ResultWriter& nextToggle(bool isActive);
	ResultWriter& endToggle();
	ResultWriter& beginSelectBtn(const char* name, int idx, bool toggleChildren);
	ResultWriter& pExpandBtn(const char* name);
	ResultWriter& pCollapseBtn(const char* name);
	void flush();

	ResultWriter& p(const String &html);
	ResultWriter& pText(const String &text);
	inline ResultWriter& beginBox(const String &caption) {
		return beginBox(caption.c_str());
	}
	inline ResultWriter& beginSelectBtn(const String &name, int idx, bool toggleChildren) {
		return beginSelectBtn(name.c_str(), idx, toggleChildren);
	}
	inline ResultWriter& pExpandBtn(const String &name) {
		return pExpandBtn(name.c_str());
	}
	inline ResultWriter& pCollapseBtn(const String &name) {
		return pCollapseBtn(name.c_str());
	}
};

};

#endif
