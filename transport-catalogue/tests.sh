#!/bin/bash

mkdir build-tests 
cd $_ 
cmake -DCMAKE_BUILD_TYPE=Debug .. 
make -j4 && ./sprint9 < ../tsC_case1_input.txt > out.txt && diff -s -w ../tsC_case1_output1.txt out.txt
cd .. && rm -rf build-tests