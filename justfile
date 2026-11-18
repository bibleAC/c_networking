cc     := "clang"
cflags := "-std=c2y -fdefer-ts -Wall -Wextra -Wpedantic -Wno-gnu-designator -g -O0 -fsanitize=address,undefined -Ilib"

# list recipes
default:
    @just --list

# build one program: just build server
build name:
    @mkdir -p build
    {{cc}} {{cflags}} src/{{name}}.c lib/*.c -o build/{{name}}

# build every program in src/
build-all:
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p build
    for f in src/*.c; do
        n=$(basename "$f" .c)
        {{cc}} {{cflags}} "$f" lib/*.c -o "build/$n"
    done

# build and run every test in tests/
test:
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p build/tests
    for f in tests/*.c; do
        n=$(basename "$f" .c)
        {{cc}} {{cflags}} "$f" lib/*.c -o "build/tests/$n"
        echo "== $n"
        ./build/tests/$n
    done
    echo "all tests passed"

# build and run: just run client 127.0.0.1 8080
run name *args: (build name)
    ./build/{{name}} {{args}}

clean:
    rm -rf build
