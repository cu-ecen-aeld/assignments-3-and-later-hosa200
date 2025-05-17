#!/bin/sh
filesdir=$1
searchstr=$2 

if [ $# -lt 2 ]
then

    echo "Invalid number of arguments, need 2 arguments"
    echo "The argurments shall be 1)file directory"
    echo "2)string to be searched in that directiory"
    exit 1
fi


if [ -d $filesdir ]
then
    # where X is the number of files in the directory and all subdirectories
    X=$(ls -1 $filesdir | wc -l)
    # Y is the number of matching lines found in respective files, 
    #, where a matching line refers to a line which contains searchstr (and may also contain additional content).
    Y=$(grep -r -R $searchstr $filesdir | wc -l)
    echo "The number of files are $X and the number of matching lines are $Y"
    
else
    echo "$filesdir is not a directory"
    exit 1
fi