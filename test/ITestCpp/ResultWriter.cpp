/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#include "ITestCpp.h"
#include "ResultWriter.h"

using namespace ITestCpp::StrUtil;
using ITestCpp::ResultWriter;

String toggleVisStyle[]= {
	String("style='visibility:hidden; position:absolute;'"),
	String("style='visibility:visible; position:relative;'"),
};

ResultWriter::ResultWriter(const char *fname) {
	f= fopen(fname, "wb");
}

ResultWriter::~ResultWriter() {
	fclose(f);
}

ResultWriter& ResultWriter::beginPage() {
	fprintf(f,
		"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
		"<html>\n"
		"<head>\n"
		"  <title>Test Case Results</title>\n"
		"  <link rel='stylesheet' type='text/css' href='ITestCpp/html/Page.css' />\n"
		"  <link rel='stylesheet' type='text/css' href='ITestCpp/html/Widgets.css' />\n"
		"</head>\n"
		"<body>\n"
		"  <script type='text/javascript' src='ITestCpp/html/ExpandCollapse.js'></script>\n"
	);
	return *this;
}

ResultWriter& ResultWriter::endPage() {
	fprintf(f,
		"</body>\n"
		"</html>\n"
	);
	return *this;
}

// Print HTML directly to the output stream
ResultWriter& ResultWriter::p(const String &html) {
	return p(html.c_str());
}

// Print HTML directly to the output stream
ResultWriter& ResultWriter::p(const char *html) {
	fprintf(f, "%s", html);
	return *this;
}

// Print text, escaping any html characters in the way
ResultWriter& ResultWriter::pText(const String &text) {
	return pText(text.c_str());
}

// Print text, escaping any html characters in the way
ResultWriter& ResultWriter::pText(const char *text) {
	// XXX: EXTREMELY LAME IMPLEMENTATION
	fprintf(f, "<![CDATA[%s]]>", text);
	return *this;
}

// Begin a groupbox.  The caption will be displayed as a label for the box.
// Use "" or NULL to omit the caption.
ResultWriter& ResultWriter::beginBox(const char* caption) {
	p("<div class='groupbox'>\n");
	if (caption && *caption)
		p("  <span class='caption'>").pText(caption).p("</span>\n");
	return *this;
}

ResultWriter& ResultWriter::endBox() {
	return p("</div>\n");
}

ResultWriter& ResultWriter::beginToggle(const String &name, bool isActive) {
	p("<div class='container'>\n"
		"  <div class='toggle' id='").p(name).p("c0").p("' ").p(toggleVisStyle[isActive?1:0]).p(">\n");
	ToggleStack.push(ToggleEntry(name, 1));
	return *this;
}

ResultWriter& ResultWriter::nextToggle(bool isActive) {
	int idx= ToggleStack.top().count++;
	p("  </div>\n"
		"  <div class='toggle' id='").p(ToggleStack.top().name).p(StrFormat("c%d' %s>\n", idx, toggleVisStyle[isActive?1:0].c_str()));
	return *this;
}

ResultWriter& ResultWriter::endToggle() {
	ToggleStack.pop();
	p("  </div>\n</div>");
	return *this;
}

ResultWriter& ResultWriter::beginSelectBtn(const char* name, int idx, bool toggleChildren) {
	p("<a class='btn' href='javascript:SelectContent(");
	p(StrFormat("\"%s\",%d,%d)'>", name, idx, toggleChildren?1:0));
	return *this;
}

ResultWriter& ResultWriter::pExpandBtn(const char* name) {
	beginSelectBtn(name, 1, false).p("<img src='html/Plus.gif").p("' alt='expand'/></a>");
	return *this;
}

ResultWriter& ResultWriter::pCollapseBtn(const char* name) {
	beginSelectBtn(name, 0, false).p("<img src='html/Minus.gif").p("' alt='collapse'/></a>");
	return *this;
}

void ResultWriter::flush() {
	fflush(f);
}
