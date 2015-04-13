
#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>


#include "schema.h"
#include <fstream>
#include <string>

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;

/// Merge two Variant Lists.
qpid::types::Variant::List mergeList(qpid::types::Variant::List a, qpid::types::Variant::List b) {
    qpid::types::Variant::List result = a;
    result.splice(result.begin(),b);
    return result;
} 

qpid::types::Variant::Map mergeMap(qpid::types::Variant::Map a, qpid::types::Variant::Map b) {
    qpid::types::Variant::Map result = a;

    for (qpid::types::Variant::Map::const_iterator it = b.begin(); it != b.end(); it++) {
        qpid::types::Variant::Map::const_iterator it_a = result.find(it->first);
        if (it_a != result.end()) {
            if ((it_a->second.getType()==VAR_MAP) && (it->second.getType()==VAR_MAP)) {
                result[it->first] = mergeMap(it_a->second.asMap(), it->second.asMap());
            } else if ((it_a->second.getType()==VAR_LIST) && (it->second.getType()==VAR_LIST)) {
                result[it->first] = mergeList(it_a->second.asList(), it->second.asList());
            } else {
                qpid::types::Variant::List list;
                list.push_front(it->second);
                list.push_front(it_a->second);
                result[it->first] = list;
            }
        } else {
            result[it->first] = it->second;
        }

    }

    return result;

}

#ifdef __YAMLCPP05__
qpid::types::Variant::Map YAMLMapToVariantMap(const YAML::Node& node) {
    Variant::Map output;
    for(YAML::const_iterator it=node.begin();it!=node.end();++it) {
        std::string key = it->first.as<std::string>();
        std::cout << "Key: " << key << " Type: " << it->second.Type() <<  std::endl;
        switch(it->second.Type()) {
            case YAML::NodeType::Map:
                output[key] = YAMLMapToVariantMap(it->second);
                break;
            case YAML::NodeType::Sequence:
                output[key] = YAMLSequenceToVariantList(it->second);
                break;
            case YAML::NodeType::Scalar:
                try {
                    output[key] = it->second.as<std::string>();
                } catch(YAML::TypedBadConversion<std::string> e) {
                    cout << "error" << endl;
                }
                break;
        }
    }
    return output;
}

qpid::types::Variant::List YAMLSequenceToVariantList(const YAML::Node& node) {
    Variant::List output;
    for (YAML::const_iterator it=node.begin();it!=node.end();++it) {
        switch (it->Type()) {
            case YAML::NodeType::Map:
                output.push_back(YAMLMapToVariantMap(it->second));
                break;
            case YAML::NodeType::Sequence:
                output.push_back(YAMLSequenceToVariantList(it->second));
                break;
            case YAML::NodeType::Scalar:
                output.push_back(Variant(it->as<std::string>()));
                break;
        }
    }
    return output;
}

#else

Variant::Map mapToVariantMap(const YAML::Node &node) {
    Variant::Map output;

    if (node.Type() == YAML::NodeType::Map) {
        for(YAML::Iterator it=node.begin(); it!=node.end(); ++it) {
            if (it.first().Type() == YAML::NodeType::Scalar) {
                string key;
                it.first() >> key;
                if (it.second().Type() == YAML::NodeType::Map) {
                    output[key] = mapToVariantMap(it.second());
                } else if (it.second().Type() == YAML::NodeType::Sequence) {
                    output[key] = sequenceToVariantList(it.second());
                } else if (it.second().Type() == YAML::NodeType::Scalar) {
                    string value;
                    it.second() >> value;
                    output[key] = value;
                }
            } else {
                printf("Error, key is no scalar\n");
            }
        }
    }
    return output;
}

Variant::List sequenceToVariantList(const YAML::Node &node) {
    Variant::List output;

    if (node.Type() == YAML::NodeType::Sequence) {
        for(unsigned int i=0; i<node.size(); i++) {
            if (node[i].Type() == YAML::NodeType::Sequence) {
                output.push_back(sequenceToVariantList(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Map) {
                output.push_back(mapToVariantMap(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Scalar) {
                string value;
                node[i] >> value;
                output.push_back(Variant(value));
            }
        }
    }
    return output;
}
#endif

Variant::Map parseSchema(const fs::path &file) {
#ifdef __YAMLCPP05__
    YAML::Node schema = YAML::LoadFile(file.c_str());
    return YAMLMapToVariantMap(schema);
#else
    std::ifstream fin(file.c_str());
    YAML::Parser parser(fin);
    Variant::Map schema;
    YAML::Node doc;
    while(parser.GetNextDocument(doc)) {
        if (doc.Type() == YAML::NodeType::Map) {
            schema = mapToVariantMap(doc);
        }
    }
    return schema;
#endif
}

