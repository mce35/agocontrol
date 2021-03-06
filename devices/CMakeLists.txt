cmake_minimum_required (VERSION 2.6)

set (AGODEVICES
  ATAlarm
  shvt250
  onvif
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
  mopidy
  alert
  ipx800
  raspiCamera
  scheduler
  tellstick
  lifx
  knx
  agodmx
  PLCBUS
  MySensors
  rrdtool
  zmcam
  ETAtouch
  oregon
  yamaha
  gammu
  axisaudiotransmit
  weatherreporter
  MQTT
  NVR
  harmonyhub
)

FIND_PACKAGE(OpenCV)

IF (NOT OpenCV_FOUND)
    list(REMOVE_ITEM AGODEVICES
      NVR
    )
    MESSAGE(STATUS "OpenCV not found, NOT building NVR device")
ENDIF (NOT OpenCV_FOUND)

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
      onvif
      MySensors
  )
ENDIF (FREEBSD)

foreach (device ${AGODEVICES})
    # options what to build
    OPTION(BUILD_DEVICE_${device} "Build the device ${device}" ON)
    if (BUILD_DEVICE_${device})
        add_subdirectory(${device})
    endif()
endforeach(device)
