#!/bin/bash
set -e -u

: "${CLANG_FORMAT:=clang-format}"

_main() {
	git ls-files --full-name '*.cpp' '*.h' '*.inl' | parallel --quote -X --max-chars 16384 "${CLANG_FORMAT}" -i {} {}
}

_main "$@"
