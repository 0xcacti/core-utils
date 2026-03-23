#!/bin/bash

# Create test structure
mkdir -p testdir/subdir1/subsubdir
mkdir -p testdir/subdir2

# Regular files
echo "hello world" > testdir/file1.txt
echo "foo bar" > testdir/subdir1/file2.txt
echo "baz qux" > testdir/subdir1/subsubdir/file3.txt
echo "deep file" > testdir/subdir2/file4.txt

# Symlinks
ln -s ../file1.txt testdir/subdir1/link_to_file1
ln -s /tmp testdir/link_to_tmp

# File with specific permissions
echo "restricted" > testdir/restricted.txt
chmod 600 testdir/restricted.txt

echo "Created test structure:"
find testdir -ls
