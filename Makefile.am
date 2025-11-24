EXTRA_DIST = \
	assets \
	bootstrap \
	COPYING.GPLv3 \
	CREDITS \
	extras \
	radio-browser-info-api.md \
	README.md \
	TODO.org


SUBDIRS = external

external/%:
	(cd external && $(MAKE) $(AM_MAKEFLAGS))


AM_CXXFLAGS = \
	-Wall -Wextra -Werror


noinst_LIBRARIES = libnet.a

libnet_a_SOURCES = \
	src/net/address.cpp \
	src/net/address.hpp \
	src/net/error.cpp \
	src/net/error.hpp \
	src/net/resolver.cpp \
	src/net/resolver.hpp \
	src/net/socket.cpp \
	src/net/socket.hpp


noinst_PROGRAMS = radiiu.elf


radiiu_elf_SOURCES = \
	src/About.cpp \
	src/About.hpp \
	src/App.cpp \
	src/App.hpp \
	src/async_queue.hpp \
	src/Browser.cpp \
	src/Browser.hpp \
	src/byte_stream.cpp \
	src/byte_stream.hpp \
	src/cfg.cpp \
	src/cfg.hpp \
	src/decoder.cpp \
	src/decoder.hpp \
	src/decoder_aac.cpp \
	src/decoder_aac.hpp \
	src/decoder_mp3.cpp \
	src/decoder_mp3.hpp \
	src/decoder_opus.cpp \
	src/decoder_opus.hpp \
	src/decoder_vorbis.cpp \
	src/decoder_vorbis.hpp \
	src/Favorites.cpp \
	src/Favorites.hpp \
	src/http_client.cpp \
	src/http_client.hpp \
	src/humanize.cpp \
	src/humanize.hpp \
	src/IconManager.cpp \
	src/IconManager.hpp \
	src/IconsFontAwesome4.h \
	src/icy.cpp \
	src/icy.hpp \
	src/icy_stream.cpp \
	src/icy_stream.hpp \
	src/imgui_extras.cpp \
	src/imgui_extras.hpp \
	src/json.cpp \
	src/json.hpp \
	src/main.cpp \
	src/Player.cpp \
	src/Player.hpp \
	src/radio_client.cpp \
	src/radio_client.hpp \
	src/Recent.cpp \
	src/Recent.hpp \
	src/rest.cpp \
	src/rest.hpp \
	src/Settings.cpp \
	src/Settings.hpp \
	src/Station.cpp \
	src/Station.hpp \
	src/stdout-wiiu.cpp \
	src/stream_metadata.cpp \
	src/stream_metadata.hpp \
	src/TabID.cpp \
	src/TabID.hpp \
	src/thread_safe.hpp \
	src/ui.cpp \
	src/ui.hpp \
	src/utils.cpp \
	src/utils.hpp


radiiu_elf_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CURL_CFLAGS) \
	$(JANSSON_CFLAGS) \
	$(MPG123_CFLAGS) \
	$(SDL_CFLAGS) \
	$(FAAD2_CFLAGS) \
	$(FT_CFLAGS) \
	$(OPUSFILE_CFLAGS) \
	$(VORBISFILE_CFLAGS) \
	-I$(srcdir)/external/curlxx/include \
	-DIMGUI_USER_CONFIG='"$(srcdir)/external/custom_imconfig.h"' \
	-I$(srcdir)/external/imgui \
	-I$(srcdir)/external/imgui/misc/cpp \
	-I$(srcdir)/external/mpg123xx/include \
	-I$(srcdir)/external/sdl2xx/include


radiiu_elf_LDADD =  \
	libnet.a \
	$(LDADD) \
	external/curlxx/libcurlxx.a \
	external/libimgui.a \
	external/mpg123xx/libmpg123xx.a \
	external/sdl2xx/libsdl2xx_image.a \
	external/sdl2xx/libsdl2xx.a \
	$(CURL_LIBS) \
	$(FAAD2_LIBS) \
	$(JANSSON_LIBS) \
	$(MPG123_LIBS) \
	$(OPUSFILE_LIBS) \
	$(VORBISFILE_LIBS) \
	$(SDL_LIBS) \
	$(FT_LIBS)


if ENABLE_WIIU

radiiu_elf_LDFLAGS = \
	$(AM_LDFLAGS) \
	-Wl,-Map,radiiu.map

RPX_TARGET = radiiu.rpx

WUHB_FILE = radiiu.wuhb

$(WUHB_FILE): $(RPX_TARGET) $(srcdir)/assets/meta/icon.png
	$(WUHBTOOL) \
		$< $@ \
		--content=$(srcdir)/assets/content \
		--icon=$(srcdir)/assets/meta/icon.png \
		--tv-image=$(srcdir)/assets/meta/splash-tv.png \
		--drc-image=$(srcdir)/assets/meta/splash-drc.png \
		--name="$(PACKAGE_STRING)" \
		--short-name="$(PACKAGE_TARNAME)"

all-local: $(WUHB_FILE)

PHONY = run
run: $(WUHB_FILE)
	WIILOAD=tcp:wiiu wiiload $<


install-wiiu: $(WUHB_FILE)
	curl --upload-file $< "ftp://wiiu:/fs/vol/external01/wiiu/apps/"


uninstall-wiiu:
	curl "ftp://wiiu" --quote "DELE /fs/vol/external01/wiiu/apps/$(WUHB_FILE)"


CLEANFILES = $(WUHB_FILE) radiiu.map

else !ENABLE_WIIU

PHONY = run

run: radiiu.elf
	cd $(top_srcdir) && $(abs_top_builddir)/$<

endif !ENABLE_WIIU


@INC_AMINCLUDE@


.PHONY: company
company: compile_flags.txt

compile_flags.txt: Makefile
	printf "%s" "$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(radiiu_elf_CPPFLAGS)" | xargs -n1 | sort -u > compile_flags.txt
	$(CPP) -xc++ /dev/null -E -Wp,-v 2>&1 | sed -n 's,^ ,-I,p' >> compile_flags.txt


DISTCLEANFILES = \
	$(AMINCLUDE) \
	compile_flags.txt
