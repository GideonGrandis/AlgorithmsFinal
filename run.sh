#!/bin/sh
make algorithm.cpp
./algorithm $1 $2 $3
perl scripts/is_valid.pl $1 $2 $3
