autogen_files: $(autogen_src) $(unittest_autogen_tests)

unittest.c $(unittest_autogen_tests): $(build_script_dir)/UnitTestGen.pm $(unittest_autoscan)
	perl -I$(build_script_dir) -MUnitTestGen -e UnitTestGen::run -- --entrypoint unittest.c --testdir t $(unittest_autoscan)

.PHONY: autogen_files
