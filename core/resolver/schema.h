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

qpid::types::Variant::List mergeList(qpid::types::Variant::List a, qpid::types::Variant::List b);
qpid::types::Variant::Map mergeMap(qpid::types::Variant::Map a, qpid::types::Variant::Map b);
Variant::List sequenceToVariantList(const YAML::Node &node);
Variant::Map mapToVariantMap(const YAML::Node &node);
Variant::Map parseSchema(const char *filename);

