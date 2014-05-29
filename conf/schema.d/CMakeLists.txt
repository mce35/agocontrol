cmake_minimum_required (VERSION 2.6)

file (GLOB SCHEMA_FILES *.yaml)

install (FILES ${SCHEMA_FILES} DESTINATION ${CONFDIR}/schema.d)
