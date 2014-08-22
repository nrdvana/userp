/**
 * Intellitree Test case system for Cpp.
 * Copyright (C) 2007 Intellitree Solutions llc.
 *
 * This library aids in quickly writing test cases in C++, running them, and
 * analyzing the results along with a convenient stack dump from gdb.
 */
#ifndef ITESTCPP_H
#define ITESTCPP_H

#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <string>

namespace ITestCpp {

class TestEntity {
	const char* const _name;
public:
	TestEntity(const char* name): _name(name) {}
	const char* GetName() const {
		return _name;
	}
	template <class T>
	bool is() const {
		return dynamic_cast<T*>(this);
	}
	template <class T>
	T& as() const {
		return &dynamic_cast<T>(*this);
	}
};

class TestCase;

class TestList: public TestEntity {
	std::vector<TestCase*> _Cases;
	std::vector<TestList*> _Lists;
public:
	TestList(const char* name, TestList *Owner= NULL): TestEntity(name), Cases(this), Lists(this) {
		if (Owner)
			Owner->add(this);
	}	

	inline void add(TestCase* tcase) {
		_Cases.push_back(tcase);
	}
	inline void add(TestList* tlist) {
		_Lists.push_back(tlist);
	}
	
	class CaseAccessor {
		TestList *Source;
	public:
		CaseAccessor(TestList* tl): Source(tl) {}
		const TestCase& operator [] (int idx) const { return *Source->_Cases[idx]; }
		const int size() const { return Source->_Cases.size(); }
	} Cases;

	class ListAccessor {
		TestList *Source;
	public:
		ListAccessor(TestList* tl): Source(tl) {}
		const TestList& operator [] (int idx) const { return *Source->_Lists[idx]; }
		const int size() const { return Source->_Lists.size(); }
	} Lists;
};
	
/** Base class for all test cases.
 * Simply goups a name and a function, and adds them to a list.
 */
class TestCase: public TestEntity {
public:
	TestCase(const char* name, TestList *Owner): TestEntity(name) {
		Owner->add(this);
	}

	virtual void Run() const= 0;
};

#define TEST_CASE(name) class name: public ITestCpp::TestCase { public: name(): ITestCpp::TestCase( #name, &_AllTestsInNamespace ) { } void Run() const; } name##_Instance; void name::Run() const

extern TestList MasterTestList;

// Namespace for assertion-related stuff.
namespace AssertUtil {

void Fail();

template<class T>
inline void AssertEQ(T Expected, T Actual) {
	if (!(Expected == Actual)) Fail();
}

template<class T>
inline void AssertNEQ(T Expected, T Actual) {
	if (!(Expected != Actual)) Fail();
}

template<class T>
inline void AssertGT(T Expected, T Actual) {
	if (!(Expected > Actual)) Fail();
}

template<class T>
inline void AssertLT(T Expected, T Actual) {
	if (!(Expected < Actual)) Fail();
}

template<class T>
inline void AssertGTE(T Expected, T Actual) {
	if (!(Expected >= Actual)) Fail();
}

template<class T>
inline void AssertLTE(T Expected, T Actual) {
	if (!(Expected <= Actual)) Fail();
}

inline void AssertTrue(bool Val) {
	if (!Val) Fail();
}

} // namespace AssertUtil

namespace StrUtil {
	typedef std::string String;
	
	String StrFormat(const char *FmtStr, ...);
} // namespace StrUtil

}

#endif
