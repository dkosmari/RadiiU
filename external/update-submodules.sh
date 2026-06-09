#!/bin/bash

GREEN=$(tput setaf 10)
YELLOW=$(tput setaf 11)
NONE=$(tput sgr0)


update_submodule()
{
    echo "${GREEN}${1}${NONE}: '${YELLOW}git -C $1 submodule update --init --recursive${NONE}'"
    git -C $1 submodule update --init --recursive || exit 2
}


update_submodule curlxx
update_submodule glaze
update_submodule imgui
update_submodule librpxloader
update_submodule mpg123xx
update_submodule sdl2xx


exit 0
