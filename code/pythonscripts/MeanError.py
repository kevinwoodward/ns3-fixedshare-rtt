# Log output parser. For retreving RTT mean error values for one run

import os
import re

pathstr = "/Users/kevin/Downloads/ns-allinone-3.30.1/ns-3.30.1/s4log.txt"

fdata = []
with open (pathstr) as file:
    fdata = file.readlines()
    
filtered = [x for x in fdata if "Mean error of" in x]

num = 0
den = 0

for line in filtered:
    (err, weight) = re.findall('.*of ([0-9.]+) with a weight of ([0-9]+)', line)[0]
    err = float(err)
    weight = float(weight)
    if (weight > 1):
        print(err, weight)
        num += (err * weight)
        den += weight

print ("Average error: ", num/den, " with a weight of: ", den)

# Average error:  48.20216695277363  with a weight of:  26680.0

