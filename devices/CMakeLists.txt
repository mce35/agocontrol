cmake_minimum_required (VERSION 2.6)

set (AGODEVICES
  ATAlarm
  example
  enocean3
  blinkm
  chromoflex
  firmata
  i2c
  irtrans_ethernet
  kwikwai
  mediaproxy
  rain8net
  zwave
  1wire
  syslog
  enigma2
  asterisk
  agoapc
  agojointspace
  raspiGPIO
  raspi1wGPIO
  raspiMCP3xxxGPIO
  gc100
  onkyo
  x10
  webcam
  temperatur.nu
  wake_on_lan
  squeezebox
  alert
  ipx800
  raspiCamera
  scheduler
  knx
  agodmx
  PLCBUS
  MySensors
  rrdtool
  zmcam
  ETAtouch
)

IF (FREEBSD)
	# These does not build on FreeBSD currently
	list(REMOVE_ITEM AGODEVICES
	  blinkm
	  firmata
	  i2c
	  irtrans_ethernet
	  knx
	  agodmx
	  PLCBUS
	  zmcam
  )
ENDIF (FREEBSD)

foreach (device ${AGODEVICES})
    # options what to build
    OPTION(BUILD_DEVICE_${device} "Build the device ${device}" ON)
    if (BUILD_DEVICE_${device})
        add_subdirectory(${device})
    endif()
endforeach(device)
