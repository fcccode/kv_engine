#!/usr/bin/env python2.7

# Script to show which commit(s) are not yet merged between our release branches.

from __future__ import print_function
import subprocess
import sys

class bcolors:
    """Define ANSI color codes, if we're running under a TTY."""
    if sys.stdout.isatty():
        HEADER = '\033[36m'
        WARNING = '\033[33m'
        ENDC = '\033[0m'
    else:
        HEADER = ''
        WARNING = ''
        ENDC = ''

# Sequences of branches to check for unmerged patches. Each toplevel
# element is a series of branches (ordered by ancestory) which should
# be merged into each other.  i.e. the oldest supported branch to the
# newest, which is the order patches should be merged.
#
# There is a single sequence of branches for branches representing
# whole release "trains" - for example vulcan is a train of (5.5.0,
# 5.5.1, 5.5.2) and should be kept merged into alice (6.0.0, 6.0.1,
# 6.0.2, ...), as new maintenance releases come along.
#
# However, we also have specific branches for a single release
# (e.g. 6.5.0) are of limited lifespan - once 6.5.0 has shipped the
# branch will not change and future fixes from say alice which need to
# be included in 6.5.x should be merged into the release train branch
# (e.g. mad-hatter).
#
# As such, there are multiple (currently two) sequence of branches -
# one for the main release trains and (currently) one for 6.5.0 in
# particular.
sequences = (
    # main release train sequence
    {('couchbase/watson_ep',
      'couchbase/spock'),
     ('couchbase/watson_mc',
      'couchbase/spock'),
     ('couchbase/spock',
      'couchbase/vulcan'),
     ('couchbase/vulcan',
      'couchbase/alice'),
     ('couchbase/alice',
      'couchbase/mad-hatter'),
     ('couchbase/mad-hatter',
      'couchbase/master')},
    # 6.5.0 specific branch
    {('couchbase/6.5.0',
      'couchbase/mad-hatter')})

total_unmerged = 0
for sequence in sequences:
    for (downstream, upstream) in sequence:
        commits = subprocess.check_output(['git', 'cherry', '-v',
                                           upstream, downstream])
        count = len(commits.splitlines())
        total_unmerged += count
        if count > 0:
            print((bcolors.HEADER +
                   "{} commits in '{}' not present in '{}':" +
                   bcolors.ENDC).format(count, downstream, upstream))
            print(commits)

if total_unmerged:
    print((bcolors.WARNING + "Total of {} commits outstanding" +
           bcolors.ENDC).format(total_unmerged))

sys.exit(total_unmerged)
