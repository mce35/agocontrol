#ifndef SCHEMA_H
#define SCHEMA_H

#include <boost/filesystem.hpp>

#ifdef __FreeBSD__
/* YAML-CPP 0.3 is required, FreeBSD port devel/yaml-cpp03 installs this in
 * a custom location to allow setups with both 0.3 and 0.5 */
#include "yaml-cpp03/yaml.h"
#else
#include "yaml-cpp/yaml.h"
#endif

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

qpid::types::Variant::List mergeList(qpid::types::Variant::List a, qpid::types::Variant::List b);
qpid::types::Variant::Map mergeMap(qpid::types::Variant::Map a, qpid::types::Variant::Map b);
Variant::Map parseSchema(const fs::path &filename);

// the YAML-CPP API did change between 0.3 and 0.5
#ifdef __YAMLCPP05__
qpid::types::Variant::List YAMLSequenceToVariantList(const YAML::Node &node);
qpid::types::Variant::Map YAMLMapToVariantMap(const YAML::Node &node);
#else
Variant::List sequenceToVariantList(const YAML::Node &node);
Variant::Map mapToVariantMap(const YAML::Node &node);
#endif

#endif
