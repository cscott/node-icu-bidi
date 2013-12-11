import sys
import os
from subprocess import check_call

confdir = os.path.abspath(sys.argv[1])
print "Changing to", confdir
os.chdir(confdir)

print sys.argv[2:]
check_call(['bash', './configure'] + sys.argv[2:])

sys.exit(0)
