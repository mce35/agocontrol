cmake_minimum_required (VERSION 2.6)

#get files
FILE(GLOB FILES_WSDL wsdl/*.*)
FILE(GLOB FILES_ONVIF onvif/*.*)
FILE(GLOB FILES_DIRS_SUDS suds/*)
FILE(GLOB FILES_SUDS_PASSWORDDIGEST suds_passworddigest/*.*)

#install files
install (PROGRAMS agoonvif.py DESTINATION ${BINDIR})
#onvif lib
install (DIRECTORY DESTINATION ${BINDIR}/onvif)
install (PROGRAMS ${FILES_ONVIF} DESTINATION ${BINDIR}/onvif)
#wsdl
install (DIRECTORY DESTINATION /etc/onvif/wsdl)
install (PROGRAMS ${FILES_WSDL} DESTINATION /etc/onvif/wsdl)
#suds lib
foreach(ITEM ${FILES_DIRS_SUDS})
    if( IS_DIRECTORY "${ITEM}" )
        list( APPEND DIRS_SUDS "${ITEM}" )
    else()
        list( APPEND FILES_SUDS "${ITEM}" )
    endif()
endforeach()
install( FILES ${FILES_SUDS} DESTINATION ${BINDIR}/suds )
install( DIRECTORY ${DIRS_SUDS} DESTINATION ${BINDIR}/suds )
#suds_passworddigest lib
install (DIRECTORY DESTINATION ${BINDIR}/suds_passworddigest)
install (PROGRAMS ${FILES_SUDS_PASSWORDDIGEST} DESTINATION ${BINDIR}/suds_passworddigest)

