#!/bin/bash
echo "First Arg: $1"

make "algorithm"
make "parseToCsv"

./parseToCsv $1 "Timeslots, Rooms, Classes, Professors, Students, Runtime, Percentage of classes assigned, Student preference value, Student preference maximum, Fit percentage, e1-Percentage, e2-Percentage, e2-Limit, e3-Action, e4-Increase, e5-Limit, e5-Number of sections made"
perl brynmawr/get_bmc_info.py $1 "dataPrefs.txt" "dataConsts.txt"

./algorithm "dataConsts.txt" "dataPrefs.txt" -d | xargs ./parseToCsv $1

./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e1 0 | xargs ./parseToCsv $1

for i in {5..95..5}
do
  ./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e1 $i | xargs ./parseToCsv $1
done

./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e2 0 60 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e2 0 120 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e2 0 180 | xargs ./parseToCsv $1
for i in {5..95..5}
do
  for j in {60..180..60}
  do
    ./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e2 $i $j | xargs ./parseToCsv $1
  done
done

./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e3 0 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e3 1 | xargs ./parseToCsv $1

./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 -100 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 -10 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 -5 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 -1 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 1 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 5 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 10 | xargs ./parseToCsv $1
./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e4 100 | xargs ./parseToCsv $1

for i in {5..100..5}
do 
  ./algorithm "dataConsts.txt" "dataPrefs.txt" -d -e5 $i | xargs ./parseToCsv $1
done
