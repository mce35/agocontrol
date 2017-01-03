cmake_minimum_required (VERSION 2.6)

# Files needed for lifxlan
# Installing local copy of lifxlan for now. CDhnages accepted by maintainer but still not available in standard package
# https://github.com/mclarkk/lifxlan
FILE(GLOB LIFXLAN_FILES lifxlan/*.*)

install (PROGRAMS agolifx.py DESTINATION ${BINDIR})
install (PROGRAMS lifxbase.py DESTINATION ${BINDIR})
install (PROGRAMS lifxnet.py DESTINATION ${BINDIR})
install (PROGRAMS lifxlan2.py DESTINATION ${BINDIR})
install (FILES    prodinfo.json DESTINATION ${BINDIR})

# Install files for lifxlan in subdir
install (DIRECTORY DESTINATION ${BINDIR}/lifxlan)
install (PROGRAMS ${LIFXLAN_FILES} DESTINATION ${BINDIR}/lifxlan)

#install (PROGRAMS lifxlan/__init.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/device.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/lifxlan.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/light.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/message.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/msgtype.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/multizonelight.py lifxnet.py DESTINATION ${LIFXLAN})
#install (PROGRAMS lifxlan/unpack.py lifxnet.py DESTINATION ${LIFXLAN})
