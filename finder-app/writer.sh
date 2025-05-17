#!/bin/bash

writefile=$1
writestr=$2

if [ $# -lt 2 ]; then
    echo "Invalid number of arguments, need 2 arguments"
    echo "The arguments shall be:"
    echo "  1) A full path to a file (including filename) on the filesystem"
    echo "  2) A text string which will be written within this file"
    exit 1
fi

# Extract the directory path from the full file path
dirpath=$(dirname "$writefile")

# Create the directory if it doesn't exist
if [ ! -d "$dirpath" ]; then
    mkdir -p "$dirpath"
fi

# Write the string to the file (overwrite)
echo "$writestr" > "$writefile"
