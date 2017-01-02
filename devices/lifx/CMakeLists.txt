cmake_minimum_required (VERSION 2.6)

set (LIFXLAN ${BINDIR}/lifxlan CACHE STRING "Directory for lifxlan files")

# Files needed for lifxlan
FILE(GLOB LIFXLAN_FILES lifxlan/*.*)

install (PROGRAMS agolifx.py DESTINATION ${BINDIR})
install (PROGRAMS lifxbase.py DESTINATION ${BINDIR})
install (PROGRAMS lifxnet.py DESTINATION ${BINDIR})
install (PROGRAMS lifxlan2.py DESTINATION ${BINDIR})

# Install files for lifxlan in subdir
install (DIRECTORY DESTINATION ${LIFXLAN})
install (FILES ${LIFXLAN_FILES} DESTINATION ${LIFXLAN})

#install (PROGRAMS lifxlan/__init.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/device.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/lifxlan.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/light.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/message.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/msgtype.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/multizonelight.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/unpack.py lifxnet.py DESTINATION ${LIFXLAN})
