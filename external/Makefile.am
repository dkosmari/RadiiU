SUBDIRS = \
	curlxx \
	mpg123xx \
	sdl2xx


AM_CXXFLAGS = \
	-Wall -Wextra -Werror


noinst_LIBRARIES = \
	libimgui.a


libimgui_a_SOURCES = \
	custom_imgui.h \
	imgui/backends/imgui_impl_sdl2.h \
	imgui/backends/imgui_impl_sdl2.cpp \
	imgui/backends/imgui_impl_sdlrenderer2.cpp \
	imgui/backends/imgui_impl_sdlrenderer2.h \
	imgui/imconfig.h \
	imgui/imgui.cpp \
	imgui/imgui.h \
	imgui/imgui_draw.cpp \
	imgui/imgui_internal.h \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/imstb_rectpack.h \
	imgui/imstb_textedit.h \
	imgui/misc/cpp/imgui_stdlib.cpp \
	imgui/misc/cpp/imgui_stdlib.h \
	imgui/misc/freetype/imgui_freetype.cpp \
	imgui/misc/freetype/imgui_freetype.h


libimgui_a_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DIMGUI_USER_CONFIG='"$(abs_srcdir)/custom_imconfig.h"' \
	-I$(srcdir)/imgui \
	$(SDL_CFLAGS) \
	$(FT_CFLAGS)


.PHONY: company
company: compile_flags.txt

compile_flags.txt: Makefile
	printf "%s" "$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS)" | xargs -n1 | sort -u > $(srcdir)/compile_flags.txt
	$(CPP) -xc++ /dev/null -E -Wp,-v 2>&1 | sed -n 's,^ ,-I,p' >> $(srcdir)/compile_flags.txt


DISTCLEANFILES = compile_flags.txt
