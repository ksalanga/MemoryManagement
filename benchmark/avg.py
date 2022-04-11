import sys

total = 0
i = 0
with open('{}.txt'.format(sys.argv[1]), 'r') as inp:
   for line in inp:
       try:
           num = float(line)
           total += num
           i += 1
       except ValueError:
           print('{} is not a number!'.format(line))

with open('{}_avg.txt'.format(sys.argv[1]), 'w') as outp:
    outp.write('Total Lines: {}\n'.format(i))
    outp.write('Average: {}%\n'.format(total / i))