#!/bin/bash
rm -rf release
mkdir -p release

cp -rf Hrv *.{hpp,cpp,txt,json} LICENSE release/

mv release score-addon-hrv
7z a score-addon-hrv.zip score-addon-hrv
