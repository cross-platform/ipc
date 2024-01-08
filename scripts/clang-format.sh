#!/bin/bash

DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
cd "$DIR/.."

find . -iname *.h -o -iname *.cpp | grep -v subprojects | xargs clang-format --style=file --verbose -i
