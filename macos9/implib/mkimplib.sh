#!/bin/sh
# mkimplib.sh LIBNAME SYMBOLS.txt OUT.a - a weak import library for a classic
# Mac OS shared library, as Retro68's MakeImport makes them from the library
# itself: an XCOFF shared object exporting a stub per function. The archive
# member is named LIBNAME__weak.o, which MakePEF reads as "import LIBNAME,
# weakly": the application still launches where the library is missing, with
# the functions' addresses null.
set -eu
lib=$1 syms=$2 out=$3
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
{
    printf '\t.toc\n'
    grep -v '^#' "$syms" | grep . | while read -r s; do
        printf '.text\n\t.globl %s\n\t.globl .%s\n\t.csect %s[DS]\n%s:\n.long .%s, TOC[tc0], 0\n.text\n.%s:\n\tblr\n' \
            "$s" "$s" "$s" "$s" "$s" "$s"
    done
} > "$tmp/stub.s"
grep -v '^#' "$syms" | grep . > "$tmp/stub.exp"
powerpc-apple-macos-as "$tmp/stub.s" -o "$tmp/stub.o"
powerpc-apple-macos-ld -shared --no-check-sections -bexport:"$tmp/stub.exp" -o "$tmp/${lib}__weak.o" "$tmp/stub.o"
rm -f "$out"
powerpc-apple-macos-ar cq "$out" "$tmp/${lib}__weak.o"
