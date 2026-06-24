#!/usr/bin/env bash
set -euo pipefail

DSYNC="./dsync"

PASS_COUNT=0

fail() {
    echo "[FAIL] $1"
    exit 1
}

pass() {
    echo "[PASS] $1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

new_workdir() {
    mktemp -d
}

verify_trees_equal() {
    local src="$1"
    local dst="$2"

    diff -r "$src" "$dst" 
}

test_basic_sync() {
    local work
    work=$(new_workdir)

    
    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src/a/b"

    echo "hello" > "$src/file1.txt"
    echo "world" > "$src/a/file2.txt"

    "$DSYNC" "$src" "$dst"

    verify_trees_equal "$src" "$dst/src"

    rm -rf "$work"
    pass "basic sync"
}

test_nested_directories() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src/1/2/3/4/5"

    echo "deep" > "$src/1/2/3/4/5/file.txt"

    "$DSYNC" "$src" "$dst"

    verify_trees_equal "$src" "$dst/src"

    rm -rf "$work"
    pass "nested directories"
}

test_incremental_update() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src"

    echo "v1" > "$src/file.txt"

    "$DSYNC" "$src" "$dst"

    echo "v2" > "$src/file.txt"
    echo "new" > "$src/new.txt"

    "$DSYNC" "$src" "$dst"

    verify_trees_equal "$src" "$dst/src"

    rm -rf "$work"
    pass "incremental update"
}

test_symlink_file() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src"

    echo "hello" > "$src/target.txt"
    ln -s target.txt "$src/link.txt"

    "$DSYNC" "$src" "$dst"

    [ -L "$dst/src/link.txt" ] || fail "symlink not preserved"

    local src_target
    local dst_target

    src_target=$(readlink "$src/link.txt")
    dst_target=$(readlink "$dst/src/link.txt")

    [ "$src_target" = "$dst_target" ] || fail "symlink target mismatch"

    rm -rf "$work"
    pass "file symlink"
}

test_broken_symlink() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src"

    ln -s does_not_exist "$src/broken"

    "$DSYNC" "$src" "$dst"

    [ -L "$dst/src/broken" ] || fail "broken symlink missing"

    local target
    target=$(readlink "$dst/src/broken")

    [ "$target" = "does_not_exist" ] || fail "broken symlink changed"

    rm -rf "$work"
    pass "broken symlink"
}

test_large_file() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src"

    dd if=/dev/urandom \
       of="$src/random.bin" \
       bs=1M count=5 status=none

    "$DSYNC" "$src" "$dst"

    verify_trees_equal "$src" "$dst/src"

    rm -rf "$work"
    pass "large file"
}

test_random_tree() {
    local work
    work=$(new_workdir)

    local src="$work/src"
    local dst="$work/dst"

    mkdir -p "$dst"
    mkdir -p "$src"

    for d in $(seq 1 20); do
        mkdir -p "$src/dir$d/sub$((d % 5))"

        for f in $(seq 1 10); do
            head -c $((100 + RANDOM % 10000)) /dev/urandom \
                > "$src/dir$d/file$f.bin"
        done
    done

    "$DSYNC" "$src" "$dst"

    verify_trees_equal "$src" "$dst/src"

    rm -rf "$work"
    pass "random tree"
}

echo "Running sync tests..."
echo

test_basic_sync
test_nested_directories
test_incremental_update
test_symlink_file
test_broken_symlink
test_large_file
test_random_tree

echo
echo "$PASS_COUNT tests passed"
