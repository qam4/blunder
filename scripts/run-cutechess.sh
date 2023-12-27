#!/bin/bash

# This script is used to run SPRT tests to compare 2 versions of the engine
# using cutechess
# for details: https://rustic-chess.org/progress/sprt_testing.html

pushd "$(dirname ${BASH_SOURCE:0})"

# Baseline engine (old)
ENGINE_V1="../build/dev/blunder"
# New engine
ENGINE_V2="../build/rel/blunder"

mkdir -p ./output
rm -rf ./output/*
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/bin/cutechess-cli/lib ~/bin/cutechess-cli/cutechess-cli \
-engine name="engine v2" cmd=$ENGINE_V2 proto="xboard" arg="--xboard" stderr="./output/v2.err.log" \
-engine name="engine v1" cmd=$ENGINE_V1 proto="xboard" arg="--xboard" stderr="./output/v1.err.log" \
-each \
    tc=inf/10+0.1 \
    book="../books/i-gm1950.bin" \
    bookdepth=4 \
-games 2 -rounds 2500 -repeat 2 -maxmoves 200 \
-sprt elo0=0 elo1=10 alpha=0.05 beta=0.05 \
-concurrency 4 \
-ratinginterval 10 \
-pgnout "./output/sprt.pgn" \
-srand 0 \
-debug 2>&1 | tee ./output/cutechess.log

trap popd EXIT
