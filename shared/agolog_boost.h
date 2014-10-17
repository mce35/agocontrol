#ifndef AGOLOG_BOOST_H
#define AGOLOG_BOOST_H

#ifndef HAVE_BOOST_LOG
#error "Something is wrong, this file should only be included when Boost.Log is available!"
#endif

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>

#define AGO_TRACE() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::trace)
#define AGO_DEBUG() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::debug)
#define AGO_INFO() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::info)
#define AGO_WARNING() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::warning)
#define AGO_ERROR() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::error)
#define AGO_FATAL() BOOST_LOG_SEV(AGO_GET_LOGGER, agocontrol::log::fatal)

#define AGO_LOGGER_IMPL  boost::log::sources::severity_logger_mt<agocontrol::log::severity_level>

#endif
