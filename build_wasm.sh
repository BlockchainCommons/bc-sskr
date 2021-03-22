#!/bin/sh

# Installation instructions: https://emscripten.org/docs/getting_started/downloads.html
# cd emsdk
# source ./emsdk_env.sh

git submodule update --init

emcc -Os -I./deps/bc-shamir/src/ -I./deps/bc-crypto-base/src/ \
src/encoding.c deps/bc-shamir/src/shamir.c deps/bc-shamir/src/interpolate.c deps/bc-shamir/src/hazmat.c \
deps/bc-crypto-base/src/sha2.c deps/bc-crypto-base/src/hmac.c deps/bc-crypto-base/src/memzero.c \
 -s WASM=1 -o wasm/sskr.js -s NO_EXIT_RUNTIME=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" \
 -s EXPORTED_FUNCTIONS="['_sskr_count_shards', '_sskr_generate', '_sskr_combine', '_malloc']" \
 -s ALLOW_MEMORY_GROWTH=1 -s ALLOW_TABLE_GROWTH \
