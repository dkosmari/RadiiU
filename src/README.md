# Source code documentation

This is intended for developers that want to look at or contribute code.


## Build system

This project uses Automake as a build tool.

When developing, I use two build directories on the top-level dir, `build-native` and
`build-wiiu`, and run `../configure ...` from within them, to perform an out-of-tree
build.

Additional build systems can be contributed, and they should go into the `tools` directory
on the top-level dir.


## Indentation

The code follows mostly the Bjarne Stroustrup style.

Indentation is done mostly automatically by my [emacs
config](https://github.com/dkosmari/emacs-config).

While not strictly enforced by a `clang-format` action, respecting the indentation style
is encouraged.


## Systems vs Application code

- app-specific code uses `CamelCase` for namespaces and file names;

- generic systems code uses `snake_case`, like the C++ standard library.

Not all code is following this convention currently, but the plan is to eventually fix all
inconsistencies. The current violators of this rule are `cfg.[ch]pp` and `ui.[ch]pp`.

The application modules are implemented as simple namespaces, not classes, since they're
all singletons anyway.


## Header include order

This style of header inclusion is used:

```cpp
// assume this is foo.cpp

// standard library headers (sorted)
#include <algorithm>
#include <string>
#include <vector>

// POSIX/BSD/newlib headers (sorted)
#include <poll.h>
#include <sys/iosupport.h>
#include <sys/socket.h>

// platform-secific system headers (sorted)
#ifdef __WIIU__
#include <coreinit/energysaver.h>
#include <coreinit/memory.h>
#include <nn/act.h>
#else // !__WIIU__
#include <wordexp.h>
#endif

// 3rd party libraries (sorted), separated by per library (sorted)
#include <curl/curl.h>

#include <jansson.h>

#include <SDL_version.h>
#include <SDL_image.h>

#include <vorbis/codec.h>

// interface header
#include "foo.hpp"

// local headers needed for the implementation (sorted)
#include "bar.hpp"
#include "baz.hpp"
#include "meh.hpp"
```

Occasionally there will be a comment after the header, indicating why that header is
needed (when it's not obvious), like:

```cpp
#include <utility>      // move()
```

This makes it easier to remove unused headers later.


## Settings

The [`cfg.cpp`](cfg.cpp) and [`cfg.hpp`](cfg.hpp) files contain the global variables
related to application settings.

The UI for adjusting these settings is in [`Settings.cpp`](Settings.cpp).


## Submodule dependencies

See the [`external`](../external) directory on the top-level dir.


### curlxx

This is just a C++ wrapper for libcurl.


### mpg123xx

This is just a C++ wrapper for libmpg123.


### sdl2xx

This is just a C++ wrapper for SDL2.


### ImGui

The user interface is done with ImGui. There are a few patches to the SDL2 backend,
regarding input handling. The config 

Additional ImGui functions are available here in [`imgui_extras.cpp`](imgui_extras.cpp)
and [`imgui_extras.hpp`](imgui_extras.hpp). These include convenience overloads, including
for `sdl2xx` types. There's also code to handle drag scrolling.

The drag scrolling code needs improvement, as it doesn't allow dragging on top of an
actionable widget, like a button or a hyperlink. For that reason, I attempted to put the
UI elements all to the left of the screen, leaving the right side a free area to drag
scroll (since most users are right-handed, and will be scrolling more than they will be
tapping buttons.)


## Audio decoding

The audio decoders are implemented in the `decoder*.[ch]pp` sources. They're modeled after
`libmpg123`'s feeder API.


## Metadata handling

Stream metadata usually comes from an ICY stream. See
[`radio_client.cpp`](radio_client.cpp), when the server reports support for it.

ICY metadata consists of both special HTTP headers, and also a stream that alternates
audio data and metadata.

Metadata from the audio container (such as Ogg) is also supported, but it becomes
ambiguous when reporting to the user.


## Stream artwork and favicon loading

The [`IconsManager.cpp`](IconsManager.cpp) and [`IconsManager.hpp`](IconsManager.hpp)
files are responsible for keeping images cached in RAM.

This should implement a texture atlas in the future, but for now, loading each icon and
artwork as a separated texture does not cause performance issues.

All images loaded by that module are scaled down to a limit size, because too many station
owners think it's cute to set a 4k wallpaper as the favicon.


## Threading

Most of the application is single threaded, except:

- libcurl uses a threaded name resolver;

- [`Browser.cpp`](Browser.cpp) uses a worker thread to resolve a radio-browser.info
  mirror;
  
- [`IconManager.cpp`](IconManager.cpp) uses a worker thread to load images.

