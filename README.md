# RadiiU

<p align="center">
    <a href="https://hb-app.store/wiiu/RadiiU">
        <img src="assets/hbas/hbasbadge-wiiu.png" width="335" height="96">
    </a>
</p>

This is an internet radio player app for the Wii U.

It uses the station listing from [www.radio-browser.info](https://www.radio-browser.info).

![RadiiU Favorites tab](assets/hbas/radiiu-favorites.png)


## Build instructions

### Dependencies

- `ppc-freetype`

- `ppc-jansson`

- libcurl
  - Wii U curl package: https://github.com/dkosmari/wiiu-curl-package
  - Wii U mbedtls package: https://github.com/dkosmari/wiiu-mbedtls-package

- `ppc-faad2`

- `ppc-mpg123`

- `ppc-opusfile`

- `ppc-vorbisfile`

- SDL2
  - Wii U package: https://github.com/dkosmari/wiiu-sdl2-package

- SDL2_image

- SDL2_ttf

### Obtaining the source

- Via mercurial:
  - `hg clone https://github.com/dkosmari/RadiiU.git`

- Via git:
  - `git clone --recurse-submodules --shallow-submodules https://github.com/dkosmari/RadiiU.git`

After checking out the code, run:

```
./bootstrap
```

(This needs to only be done once, to create the `configure` script.)


### Wii U build

```
./configure --enable-wiiu --host=powerpc-eabi
make
```

If the Wii U is named `wiiu` in your local network, you can run:

- `make run`
  - This will use `wiiload` to run it on the Wii U without installing.

- `make wiiu-install`
  - This will install `radiiu.wuhb`, via ftp.

- `make wiiu-uninstall`
  - This will uninstall `radiiu.wuhb`, via ftp.

### Native build

This has only been tested on GNU/Linux environments so far.

```
./configure
make
make run
```

Note that the native build is meant to be run from the source code directory. It requires
a file named `CafeStd.ttf`, to be used as the main font.
