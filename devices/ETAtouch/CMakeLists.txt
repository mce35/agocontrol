cmake_minimum_required (VERSION 2.6)

set (my_device_service "agoeta.service.in")
set (my_device_config "eta.conf.in")

install (PROGRAMS agoeta.py DESTINATION ${BINDIR})
