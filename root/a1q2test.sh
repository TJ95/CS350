#!/usr/bin/env bash

printf "\n" > a1q2result.txt

COUNTER=1
TOTAL=0

while [  $COUNTER -le $1 ]; do
printf "\n"
echo "Test run:" $COUNTER "Total of:" $1
sys161 kernel-ASST1 "sp3 10 100 0 1 0;q" | egrep "Simulation" | awk '{print $2}' >> a1q2result.txt
((COUNTER++))
done

echo "Total time spend:"
awk '{ SUM += $1} END { print SUM }' a1q2result.txt

