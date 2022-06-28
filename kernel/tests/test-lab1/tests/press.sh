#! /bin/bash
while [ true ];
 do
# { time make run mainargs=16; } 
# if [ "$?" -ne 0 ]
# then
# break
# fi
{ time make comp mainargs=16; }
if [ "$?" -ne 0 ]
then
break
fi
done
#|& grep real | sed 's/real/old/g'
