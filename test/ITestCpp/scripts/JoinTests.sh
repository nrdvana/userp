#! /bin/sh
if grep -r '#include' tests >/dev/null 2>&1; then
	{
	echo ' *';
	echo ' * You have #include directives in your test cases.  Anything';
	echo ' *  defined in those files will get added to the namespace of that';
	echo ' *  test file, which is probably not what you want.';
	echo ' * Add all common includes, utility function, etc, to Common.cpp';
	echo ' *  which is sourced at the start of all test cases.';
	echo ' *';
	} >&2
fi;

IncludeTestDir() {
	local Dir=$1 DirPath=$2 ParentNS=$3 Indent=$4;
	echo "${Indent}namespace $Dir {";
	if [ -z "$ParentNS" ]; then
		echo "${Indent}  TestList &_AllTestsInNamespace= ITestCpp::MasterTestList;";
	else
		echo "${Indent}  TestList _AllTestsInNamespace(\"$Dir\", &$ParentNS::_AllTestsInNamespace);";
	fi;
	ls "$DirPath$Dir/"*.cpp 2>/dev/null | sed -e '/\/Global.cpp$/d' -e "s/.*/${Indent}  #include \"&\"/";
#	for Subdir in `find $DirPath$Dir -maxdepth 1 -mindepth 1 -type d | sed -n 's/.*\/\([^.][^/][^/]*\)/\1/p'`; do
	for Subdir in `ls $DirPath$Dir`; do
	  [[ -d "$DirPath$Dir/$Subdir" ]] && IncludeTestDir "$Subdir" "$DirPath$Dir/" "$ParentNS::$Dir" "$Indent  ";
	done;
	echo "${Indent}}";
};

cat <<END
// Automatically generated from Makefile
// Modifications will be lost on next "make check"
#include "ITestCpp.h"
namespace ITestCpp {
  TestList MasterTestList("tests");
}
END
echo "#include \"$1/Global.cpp\""
IncludeTestDir $1 "" "" ""
