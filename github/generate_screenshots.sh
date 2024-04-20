#!/bin/bash

pushd "$(dirname "$0")" >> /dev/null

files=$(ls *.png)
screenshotText="# MinUI screenshots\n\n"
addNewline=0

for file in $files; do
    screenshotText+="<img src=\"$file\" width=320 />"
    # Adds a newline every two screenshots
    if [ $addNewline -eq 0 ]; then
        screenshotText+=" "
        addNewline=1
    else
        screenshotText+="\n"
        addNewline=0
    fi
done

echo -e $screenshotText > README.md
popd >> /dev/null