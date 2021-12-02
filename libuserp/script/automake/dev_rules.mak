autogen_files: $(autogen_src) $(unittest_autogen_tests)

AM_CFLAGS += -O0 -ggdb

unittest.c $(unittest_autogen_tests): $(build_script_dir)/UnitTestGen.pm $(unittest_autoscan)
	$(PERL) $(build_script_dir)/UnitTestGen.pm --entrypoint unittest.c --testdir t $(unittest_autoscan)

.PHONY: autogen_files
