EXTRA_DIST = update-submodules.sh


SUBDIRS = \
	curlxx \
	mpg123xx \
	sdl2xx


AM_CXXFLAGS = \
	-Wall -Wextra -Werror


noinst_LTLIBRARIES =


include $(srcdir)/imgui.am

include $(srcdir)/glaze.am

include $(srcdir)/librpxloader.am


.PHONY: company
company: compile_flags.txt

compile_flags.txt: Makefile
	printf "%s" "$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS)" | xargs -n1 | sort -u > $(srcdir)/compile_flags.txt
	$(CPP) -xc++ /dev/null -E -Wp,-v 2>&1 | sed -n 's,^ ,-I,p' >> $(srcdir)/compile_flags.txt


DISTCLEANFILES = compile_flags.txt
