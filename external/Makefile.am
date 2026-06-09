LIBRPXLOADER_FILES ?= \
	$(addprefix librpxloader/,$(shell git -C $(srcdir)/librpxloader ls-files))


EXTRA_DIST = \
	update-submodules.sh \
	$(LIBRPXLOADER_FILES)

SUBDIRS = \
	curlxx \
	mpg123xx \
	sdl2xx


AM_CXXFLAGS = \
	-Wall -Wextra -Werror


noinst_LTLIBRARIES = \
	libglaze.la \
	libimgui.la


libimgui_la_SOURCES = \
	custom_imconfig.h \
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

# Uncomment this to include Style Editor window.
libimgui_la_SOURCES += imgui/imgui_demo.cpp



libimgui_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DIMGUI_USER_CONFIG='"$(srcdir)/custom_imconfig.h"' \
	-I$(srcdir)/imgui \
	$(SDL_CFLAGS) \
	$(FT_CFLAGS)


libglaze_la_SOURCES = \
	glaze/include/glaze/api/api.hpp \
	glaze/include/glaze/api/hash.hpp \
	glaze/include/glaze/api/impl.hpp \
	glaze/include/glaze/api/lib.hpp \
	glaze/include/glaze/api/std/array.hpp \
	glaze/include/glaze/api/std/deque.hpp \
	glaze/include/glaze/api/std/functional.hpp \
	glaze/include/glaze/api/std/list.hpp \
	glaze/include/glaze/api/std/map.hpp \
	glaze/include/glaze/api/std/optional.hpp \
	glaze/include/glaze/api/std/set.hpp \
	glaze/include/glaze/api/std/shared_ptr.hpp \
	glaze/include/glaze/api/std/span.hpp \
	glaze/include/glaze/api/std/string.hpp \
	glaze/include/glaze/api/std/tuple.hpp \
	glaze/include/glaze/api/std/unique_ptr.hpp \
	glaze/include/glaze/api/std/unordered_map.hpp \
	glaze/include/glaze/api/std/unordered_set.hpp \
	glaze/include/glaze/api/std/variant.hpp \
	glaze/include/glaze/api/std/vector.hpp \
	glaze/include/glaze/api/trait.hpp \
	glaze/include/glaze/api/tuplet.hpp \
	glaze/include/glaze/api/type_support.hpp \
	glaze/include/glaze/api/xxh64.hpp \
	glaze/include/glaze/base64/base64.hpp \
	glaze/include/glaze/beve.hpp \
	glaze/include/glaze/beve/beve_to_json.hpp \
	glaze/include/glaze/beve/header.hpp \
	glaze/include/glaze/beve/key_traits.hpp \
	glaze/include/glaze/beve/lazy.hpp \
	glaze/include/glaze/beve/peek_header.hpp \
	glaze/include/glaze/beve/ptr.hpp \
	glaze/include/glaze/beve/read.hpp \
	glaze/include/glaze/beve/size.hpp \
	glaze/include/glaze/beve/skip.hpp \
	glaze/include/glaze/beve/wrappers.hpp \
	glaze/include/glaze/beve/write.hpp \
	glaze/include/glaze/bson.hpp \
	glaze/include/glaze/bson/bson_to_json.hpp \
	glaze/include/glaze/bson/header.hpp \
	glaze/include/glaze/bson/read.hpp \
	glaze/include/glaze/bson/skip.hpp \
	glaze/include/glaze/bson/write.hpp \
	glaze/include/glaze/cbor.hpp \
	glaze/include/glaze/cbor/cbor_to_json.hpp \
	glaze/include/glaze/cbor/header.hpp \
	glaze/include/glaze/cbor/read.hpp \
	glaze/include/glaze/cbor/skip.hpp \
	glaze/include/glaze/cbor/wrappers.hpp \
	glaze/include/glaze/cbor/write.hpp \
	glaze/include/glaze/chrono.hpp \
	glaze/include/glaze/compare/approx.hpp \
	glaze/include/glaze/compare/compare.hpp \
	glaze/include/glaze/concepts/container_concepts.hpp \
	glaze/include/glaze/containers/flat_map.hpp \
	glaze/include/glaze/containers/inplace_vector.hpp \
	glaze/include/glaze/containers/ordered_map.hpp \
	glaze/include/glaze/containers/ordered_small_map.hpp \
	glaze/include/glaze/core/array_apply.hpp \
	glaze/include/glaze/core/as_array_wrapper.hpp \
	glaze/include/glaze/core/buffer_traits.hpp \
	glaze/include/glaze/core/cast.hpp \
	glaze/include/glaze/core/chrono.hpp \
	glaze/include/glaze/core/common.hpp \
	glaze/include/glaze/core/constraint.hpp \
	glaze/include/glaze/core/context.hpp \
	glaze/include/glaze/core/convert_struct.hpp \
	glaze/include/glaze/core/custom.hpp \
	glaze/include/glaze/core/custom_meta.hpp \
	glaze/include/glaze/core/error_category.hpp \
	glaze/include/glaze/core/feature_test.hpp \
	glaze/include/glaze/core/istream_buffer.hpp \
	glaze/include/glaze/core/manage.hpp \
	glaze/include/glaze/core/meta.hpp \
	glaze/include/glaze/core/optimization_level.hpp \
	glaze/include/glaze/core/opts.hpp \
	glaze/include/glaze/core/ostream_buffer.hpp \
	glaze/include/glaze/core/ptr.hpp \
	glaze/include/glaze/core/read.hpp \
	glaze/include/glaze/core/reflect.hpp \
	glaze/include/glaze/core/seek.hpp \
	glaze/include/glaze/core/std_error_code.hpp \
	glaze/include/glaze/core/streaming_state.hpp \
	glaze/include/glaze/core/to.hpp \
	glaze/include/glaze/core/traits.hpp \
	glaze/include/glaze/core/wrapper_traits.hpp \
	glaze/include/glaze/core/wrappers.hpp \
	glaze/include/glaze/core/write.hpp \
	glaze/include/glaze/core/write_chars.hpp \
	glaze/include/glaze/csv.hpp \
	glaze/include/glaze/csv/read.hpp \
	glaze/include/glaze/csv/skip.hpp \
	glaze/include/glaze/csv/write.hpp \
	glaze/include/glaze/eetf.hpp \
	glaze/include/glaze/eetf/cmp.hpp \
	glaze/include/glaze/eetf/defs.hpp \
	glaze/include/glaze/eetf/eetf_to_json.hpp \
	glaze/include/glaze/eetf/ei.hpp \
	glaze/include/glaze/eetf/opts.hpp \
	glaze/include/glaze/eetf/read.hpp \
	glaze/include/glaze/eetf/types.hpp \
	glaze/include/glaze/eetf/wrappers.hpp \
	glaze/include/glaze/eetf/write.hpp \
	glaze/include/glaze/exceptions/binary_exceptions.hpp \
	glaze/include/glaze/exceptions/cbor_exceptions.hpp \
	glaze/include/glaze/exceptions/core_exceptions.hpp \
	glaze/include/glaze/exceptions/csv_exceptions.hpp \
	glaze/include/glaze/exceptions/json_exceptions.hpp \
	glaze/include/glaze/exceptions/json_schema_exceptions.hpp \
	glaze/include/glaze/exceptions/jsonb_exceptions.hpp \
	glaze/include/glaze/exceptions/msgpack_exceptions.hpp \
	glaze/include/glaze/ext/cli_menu.hpp \
	glaze/include/glaze/ext/eigen.hpp \
	glaze/include/glaze/ext/glaze_asio.hpp \
	glaze/include/glaze/ext/jsonrpc.hpp \
	glaze/include/glaze/file/file_ops.hpp \
	glaze/include/glaze/file/file_watch.hpp \
	glaze/include/glaze/file/hostname_include.hpp \
	glaze/include/glaze/file/raw_or_file.hpp \
	glaze/include/glaze/file/read_directory.hpp \
	glaze/include/glaze/file/write_directory.hpp \
	glaze/include/glaze/format/format_to.hpp \
	glaze/include/glaze/forward.hpp \
	glaze/include/glaze/glaze.hpp \
	glaze/include/glaze/glaze_exceptions.hpp \
	glaze/include/glaze/hardware/volatile_array.hpp \
	glaze/include/glaze/hash/sweethash.hpp \
	glaze/include/glaze/json.hpp \
	glaze/include/glaze/json/escape_unicode.hpp \
	glaze/include/glaze/json/flatten_map.hpp \
	glaze/include/glaze/json/float_format.hpp \
	glaze/include/glaze/json/generic.hpp \
	glaze/include/glaze/json/generic_fwd.hpp \
	glaze/include/glaze/json/invoke.hpp \
	glaze/include/glaze/json/jmespath.hpp \
	glaze/include/glaze/json/json_concepts.hpp \
	glaze/include/glaze/json/json_format.hpp \
	glaze/include/glaze/json/json_ptr.hpp \
	glaze/include/glaze/json/json_stream.hpp \
	glaze/include/glaze/json/lazy.hpp \
	glaze/include/glaze/json/max_write_precision.hpp \
	glaze/include/glaze/json/minify.hpp \
	glaze/include/glaze/json/ndjson.hpp \
	glaze/include/glaze/json/patch.hpp \
	glaze/include/glaze/json/prettify.hpp \
	glaze/include/glaze/json/ptr.hpp \
	glaze/include/glaze/json/raw_string.hpp \
	glaze/include/glaze/json/read.hpp \
	glaze/include/glaze/json/schema.hpp \
	glaze/include/glaze/json/skip.hpp \
	glaze/include/glaze/json/study.hpp \
	glaze/include/glaze/json/wrappers.hpp \
	glaze/include/glaze/json/write.hpp \
	glaze/include/glaze/jsonb.hpp \
	glaze/include/glaze/jsonb/header.hpp \
	glaze/include/glaze/jsonb/jsonb_to_json.hpp \
	glaze/include/glaze/jsonb/read.hpp \
	glaze/include/glaze/jsonb/skip.hpp \
	glaze/include/glaze/jsonb/text_decode.hpp \
	glaze/include/glaze/jsonb/write.hpp \
	glaze/include/glaze/msgpack.hpp \
	glaze/include/glaze/msgpack/common.hpp \
	glaze/include/glaze/msgpack/read.hpp \
	glaze/include/glaze/msgpack/skip.hpp \
	glaze/include/glaze/msgpack/write.hpp \
	glaze/include/glaze/net/cors.hpp \
	glaze/include/glaze/net/http.hpp \
	glaze/include/glaze/net/http_client.hpp \
	glaze/include/glaze/net/http_router.hpp \
	glaze/include/glaze/net/http_server.hpp \
	glaze/include/glaze/net/openapi.hpp \
	glaze/include/glaze/net/rest_registry_impl.hpp \
	glaze/include/glaze/net/url.hpp \
	glaze/include/glaze/net/websocket_client.hpp \
	glaze/include/glaze/net/websocket_connection.hpp \
	glaze/include/glaze/record/recorder.hpp \
	glaze/include/glaze/reflection/get_name.hpp \
	glaze/include/glaze/reflection/requires_key.hpp \
	glaze/include/glaze/reflection/to_tuple.hpp \
	glaze/include/glaze/rpc/jsonrpc_registry_impl.hpp \
	glaze/include/glaze/rpc/registry.hpp \
	glaze/include/glaze/rpc/repe/buffer.hpp \
	glaze/include/glaze/rpc/repe/header.hpp \
	glaze/include/glaze/rpc/repe/plugin_helper.hpp \
	glaze/include/glaze/rpc/repe/repe.hpp \
	glaze/include/glaze/rpc/repe/repe_registry_impl.hpp \
	glaze/include/glaze/rpc/repe/repe_to_jsonrpc.hpp \
	glaze/include/glaze/simd/avx.hpp \
	glaze/include/glaze/simd/neon.hpp \
	glaze/include/glaze/simd/simd.hpp \
	glaze/include/glaze/simd/sse.hpp \
	glaze/include/glaze/stencil/stencil.hpp \
	glaze/include/glaze/stencil/stencilcount.hpp \
	glaze/include/glaze/thread/async.hpp \
	glaze/include/glaze/thread/async_string.hpp \
	glaze/include/glaze/thread/async_vector.hpp \
	glaze/include/glaze/thread/atomic.hpp \
	glaze/include/glaze/thread/guard.hpp \
	glaze/include/glaze/thread/shared_async_map.hpp \
	glaze/include/glaze/thread/shared_async_vector.hpp \
	glaze/include/glaze/thread/threadpool.hpp \
	glaze/include/glaze/thread/value_proxy.hpp \
	glaze/include/glaze/toml.hpp \
	glaze/include/glaze/toml/common.hpp \
	glaze/include/glaze/toml/opts.hpp \
	glaze/include/glaze/toml/read.hpp \
	glaze/include/glaze/toml/skip.hpp \
	glaze/include/glaze/toml/write.hpp \
	glaze/include/glaze/trace/trace.hpp \
	glaze/include/glaze/tuplet/tuple.hpp \
	glaze/include/glaze/util/atoi.hpp \
	glaze/include/glaze/util/bit.hpp \
	glaze/include/glaze/util/bit_array.hpp \
	glaze/include/glaze/util/buffer_pool.hpp \
	glaze/include/glaze/util/compare.hpp \
	glaze/include/glaze/util/convert.hpp \
	glaze/include/glaze/util/dump.hpp \
	glaze/include/glaze/util/env.hpp \
	glaze/include/glaze/util/expected.hpp \
	glaze/include/glaze/util/fast_float.hpp \
	glaze/include/glaze/util/for_each.hpp \
	glaze/include/glaze/util/glaze_fast_float.hpp \
	glaze/include/glaze/util/help.hpp \
	glaze/include/glaze/util/inline.hpp \
	glaze/include/glaze/util/itoa.hpp \
	glaze/include/glaze/util/itoa_40kb.hpp \
	glaze/include/glaze/util/key_transformers.hpp \
	glaze/include/glaze/util/memory_pool.hpp \
	glaze/include/glaze/util/murmur.hpp \
	glaze/include/glaze/util/nullable_traits.hpp \
	glaze/include/glaze/util/parse.hpp \
	glaze/include/glaze/util/primes_64.hpp \
	glaze/include/glaze/util/progress_bar.hpp \
	glaze/include/glaze/util/simple_float.hpp \
	glaze/include/glaze/util/string_literal.hpp \
	glaze/include/glaze/util/tuple.hpp \
	glaze/include/glaze/util/type_traits.hpp \
	glaze/include/glaze/util/uuid.hpp \
	glaze/include/glaze/util/validate.hpp \
	glaze/include/glaze/util/variant.hpp \
	glaze/include/glaze/util/zmij.hpp \
	glaze/include/glaze/version.hpp \
	glaze/include/glaze/yaml.hpp \
	glaze/include/glaze/yaml/common.hpp \
	glaze/include/glaze/yaml/opts.hpp \
	glaze/include/glaze/yaml/read.hpp \
	glaze/include/glaze/yaml/skip.hpp \
	glaze/include/glaze/yaml/write.hpp


if ENABLE_WIIU

noinst_LTLIBRARIES += librpxloader.la

librpxloader_la_CPPFLAGS = -I$(srcdir)/librpxloader/include

librpxloader_la_SOURCES = \
	librpxloader/include/rpxloader/rpxloader.h \
	librpxloader/source/logger.h \
	librpxloader/source/rpxloader.cpp

endif ENABLE_WIIU



.PHONY: company
company: compile_flags.txt

compile_flags.txt: Makefile
	printf "%s" "$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS)" | xargs -n1 | sort -u > $(srcdir)/compile_flags.txt
	$(CPP) -xc++ /dev/null -E -Wp,-v 2>&1 | sed -n 's,^ ,-I,p' >> $(srcdir)/compile_flags.txt


DISTCLEANFILES = compile_flags.txt
