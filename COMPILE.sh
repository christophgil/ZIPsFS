#!/usr/bin/env bash

script=$0
[[ $script != /* ]] && script=$PWD/${script#./}
${script%/*}/src/ZIPsFS.compile.sh
