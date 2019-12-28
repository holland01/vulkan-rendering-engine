#!/bin/bash

fullfile=$1
type=$2

filename=$(basename -- "$fullfile")
extension="${filename##*.}"
filename="${filename%.*}"

glslc -O0 -fshader-stage=$type $fullfile -o ${filename}.spv

