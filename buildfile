# file      : buildfile
# copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

d = butl/ tests/
./: $d doc{LICENSE NEWS README version} file{manifest}
include $d

# Don't install tests or the INSTALL file.
#
dir{tests/}: install = false
doc{INSTALL}@./: install = false
