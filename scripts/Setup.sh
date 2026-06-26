#!/bin/sh

pushd ..
vendor/bin/premake/Linux/premake5 --file=Build-PathTracing.lua gmake2
popd