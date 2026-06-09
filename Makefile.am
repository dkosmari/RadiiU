EXTRA_DIST = \
	assets \
	bootstrap \
	COPYING.GPLv3 \
	CREDITS \
	radio-browser-info-api.md \
	README.md \
	TODO.org \
	tools


SUBDIRS = external

external/%:
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C external


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
	src/rest.cpp \
	src/rest.hpp \
	src/RadioBrowserAPI.cpp \
	src/RadioBrowserAPI.hpp \
	src/AboutTab.cpp \
	src/AboutTab.hpp \
	src/App.cpp \
	src/App.hpp \
	src/async_queue.hpp \
	src/BrowserTab.cpp \
	src/BrowserTab.hpp \
	src/byte_stream.cpp \
	src/byte_stream.hpp \
	src/cfg.cpp \
	src/cfg.hpp \
	src/csv_strings.cpp \
	src/csv_strings.hpp \
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
	src/FavoritesTab.cpp \
	src/FavoritesTab.hpp \
	src/FontManager.cpp \
	src/FontManager.hpp \
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
	src/m3u.cpp \
	src/m3u.hpp \
	src/main.cpp \
	src/mime_type.cpp \
	src/mime_type.hpp \
	src/PlayerTab.cpp \
	src/PlayerTab.hpp \
	src/pls.cpp \
	src/pls.hpp \
	src/radio_client.cpp \
	src/radio_client.hpp \
	src/RecentTab.cpp \
	src/RecentTab.hpp \
	src/Serializer.hpp \
	src/SettingsTab.cpp \
	src/SettingsTab.hpp \
	src/Station.cpp \
	src/Station.hpp \
	src/StationDetailsPopup.cpp \
	src/StationDetailsPopup.hpp \
	src/stdout-wiiu.cpp \
	src/stream_metadata.cpp \
	src/stream_metadata.hpp \
	src/string_utils.cpp \
	src/string_utils.hpp \
	src/Styles.cpp \
	src/Styles.hpp \
	src/TabID.cpp \
	src/TabID.hpp \
	src/thread_safe.hpp \
	src/tracer.cpp \
	src/tracer.hpp \
	src/UI.cpp \
	src/UI.hpp \
	src/unit_test.hpp


radiiu_elf_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CURL_CFLAGS) \
	$(MPG123_CFLAGS) \
	$(SDL_CFLAGS) \
	$(FAAD2_CFLAGS) \
	$(FT_CFLAGS) \
	$(OPUSFILE_CFLAGS) \
	$(VORBISFILE_CFLAGS) \
	-I$(srcdir)/external/curlxx/include \
	-DIMGUI_USER_CONFIG='"$(srcdir)/external/custom_imconfig.h"' \
	-I$(srcdir)/external/glaze/include \
	-DGLZ_DISABLE_ALWAYS_INLINE \
	-I$(srcdir)/external/imgui \
	-I$(srcdir)/external/imgui/backends \
	-I$(srcdir)/external/imgui/misc/cpp \
	-I$(srcdir)/external/imgui/misc/freetype \
	-I$(srcdir)/external/mpg123xx/include \
	-I$(srcdir)/external/sdl2xx/include


radiiu_elf_LDADD =  \
	libnet.a \
	$(LDADD) \
	external/curlxx/lib/libcurlxx.la \
	external/libimgui.la \
	external/mpg123xx/lib/libmpg123xx.la \
	external/sdl2xx/lib/libsdl2xx_image.la \
	external/sdl2xx/lib/libsdl2xx.la \
	$(CURL_LIBS) \
	$(FAAD2_LIBS) \
	$(MPG123_LIBS) \
	$(OPUSFILE_LIBS) \
	$(VORBISFILE_LIBS) \
	$(SDL_LIBS) \
	$(FT_LIBS)


if ENABLE_WIIU

radiiu_elf_LDFLAGS = \
	$(AM_LDFLAGS) \
	-Wl,-Map,radiiu.map

radiiu_elf_CPPFLAGS += -I$(srcdir)/external/librpxloader/include

radiiu_elf_LDADD += external/librpxloader.la


RPX_FILE = radiiu.rpx

WUHB_FILE = radiiu.wuhb

$(WUHB_FILE): $(RPX_FILE) Makefile $(srcdir)/assets/meta/icon.png
	$(WUHBTOOL) \
		$< $@ \
		--content=$(srcdir)/assets/content \
		--icon=$(srcdir)/assets/meta/icon.png \
		--tv-image=$(srcdir)/assets/meta/splash-tv.png \
		--drc-image=$(srcdir)/assets/meta/splash-drc.png \
		--name="$(PACKAGE_STRING)" \
		--short-name="$(PACKAGE_TARNAME)" \
		--author="Daniel K. O."

all-local: $(WUHB_FILE)

.PHONY: run
run: all
	WIILOAD=tcp:wiiu wiiload $(WUHB_FILE)


.PHONY: wiiu-install wiiu-uninstall

wiiu-install: all
	curl --upload-file $(WUHB_FILE) "ftp://wiiu:/fs/vol/external01/wiiu/apps/"


wiiu-uninstall:
	curl "ftp://wiiu" --quote "DELE /fs/vol/external01/wiiu/apps/$(WUHB_FILE)"


CLEANFILES = $(WUHB_FILE) radiiu.map

else !ENABLE_WIIU

radiiu_elf_CPPFLAGS += $(FONTCONFIG_CFLAGS)

radiiu_elf_LDADD += $(FONTCONFIG_LIBS)


.PHONY: run

run: all
	cd $(top_srcdir) && $(abs_top_builddir)/radiiu.elf

endif !ENABLE_WIIU


@INC_AMINCLUDE@

# Uncomment this to use the non-stripped .elf
# %.rpx: %.elf
# 	$(ELF2RPL) $< $@


.PHONY: company
company: compile_flags.txt

compile_flags.txt: Makefile
	printf "%s" "$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(radiiu_elf_CPPFLAGS)" | xargs -n1 | sort -u > compile_flags.txt
	$(CPP) -xc++ /dev/null -E -Wp,-v 2>&1 | sed -n 's,^ ,-I,p' >> compile_flags.txt


DISTCLEANFILES = \
	$(AMINCLUDE) \
	compile_flags.txt
