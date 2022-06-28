#! /bin/bash
num="1"
while [ "$num" -lt 32 ];
do
echo "$num thread"
{ time make run mainargs=$num; } |& grep real | sed 's/real/new/g'
{ time make mid mainargs=$num; } |& grep real | sed 's/real/mid/g'
{ time make comp mainargs=$num; } |& grep real | sed 's/real/old/g'
{ time make double mainargs=$num; } |& grep real | sed 's/real/double/g'
{ time make fast mainargs=$num; } |& grep real | sed 's/real/fast/g'
{ time make chunk mainargs=$num; } |& grep real | sed 's/real/chunk/g'
num=$(($num + $num))
done
