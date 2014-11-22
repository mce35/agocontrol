#include <string>

#include <boost/preprocessor/stringize.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <augeas.h>

#include "agoclient.h"
#include "agoconfig.h"
#include "agolog.h"

#define MODULE_CONFDIR "/conf.d"

namespace fs = boost::filesystem;
namespace agocontrol {

augeas *augeas = NULL;

bool directories_inited = false;
fs::path config_dir;
fs::path localstate_dir;

static std::string augeasGetError() ;

fs::path prepareDirectory(const char *name, const fs::path &dir) {
    if(!fs::exists(dir)) {
        // Try to create it
        // ensureDirExists will canonical() it
        return ensureDirExists(dir);
    }
    else {
        // Exists, canonicalize it ourselfs
        return fs::canonical(dir);
    }
}

fs::path initDirectory(const char *name, const char *def){
    const char *tmp;
    std::string env ("AGO_");
    env+= name;

    boost::to_upper(env);

    fs::path dir(def);
    if((tmp = getenv(env.c_str())) != NULL) {
        dir = tmp;
    }

    try {
        dir = prepareDirectory(name, dir);
    } catch(const fs::filesystem_error& error) {
        // Canonical failed; does it not exist after all?
        AGO_WARNING() << "Failed to resolve " << name << " " << dir.string()
            << ": " << error.code().message()
            << ". Falling back to default " << def;
        dir = fs::path(def);
    }

    return dir;
}


void initDirectorys() {
    // DEFAULT_CONFDIR, DEFAULT_LOCALSTATEDIR must be set with compiler flag
    if(config_dir.empty()) {
        config_dir = initDirectory("confdir", BOOST_PP_STRINGIZE(DEFAULT_CONFDIR));
    }
    if(localstate_dir.empty()) {
        localstate_dir = initDirectory("localstatedir", BOOST_PP_STRINGIZE(DEFAULT_LOCALSTATEDIR));
    }
    directories_inited = true;
}

/* These are only callable from AgoApp and AgoClient */
void AgoClientInternal::setConfigDir(const boost::filesystem::path &dir) {
    if(!config_dir.empty()) {
        throw std::runtime_error("setConfigDir after initDirectorys was called!");
    }
    config_dir = prepareDirectory("confdir", dir);
}
void AgoClientInternal::setLocalStateDir(const boost::filesystem::path &dir) {
    if(!localstate_dir.empty()) {
        throw std::runtime_error("setLocalStateDir change after initDirectorys was called!");
    }
    localstate_dir= prepareDirectory("localstatedir", dir);
}

void AgoClientInternal::_reset() {
    // For unittest only!
    config_dir.clear();
    localstate_dir.clear();
}


fs::path ensureDirExists(const boost::filesystem::path &dir) {
    if(!fs::exists(dir))  {
        // This will throw if it fails
        fs::create_directories(dir);
    }

    // Normalize the directory; it should exist and should thus not fail
    return fs::canonical(dir);
}

fs::path ensureParentDirExists(const boost::filesystem::path &filename) {
    if(!fs::exists(filename))  {
        // Ensure parent directory exist
        fs::path dir = filename.parent_path();
        ensureDirExists(dir);
    }
    return filename;
}

fs::path getConfigPath(const fs::path &subpath) {
    if(!directories_inited) {
        initDirectorys();
    }
    fs::path res(config_dir);
    if(!subpath.empty()) {
        res /= subpath;
    }
    return res;
}

fs::path getLocalStatePath(const fs::path &subpath) {
    if(!directories_inited) {
        initDirectorys();
    }
    fs::path res(localstate_dir);
    if(!subpath.empty()) {
        res /= subpath;
    }
    return res;
}





bool augeas_init()  {
    fs::path incl_path = getConfigPath(fs::path(MODULE_CONFDIR));

    if(!fs::exists(incl_path)) {
        // Complain hard; even if we can create it, any files won't be picked up
        // after we've done aug_load. And user probably wont have any use of an empty
        // dir anyway, so tell him to fix it instead.
        AGO_ERROR() << "Cannot use " << incl_path << " as confdir, does not exist";
        return false;
    } else if (!fs::is_directory(incl_path)) {
        AGO_ERROR() << "Cannot use " << incl_path << " as confdir, not a directory";
        return false;
    }

    incl_path /= "*.conf";

    // Look for augeas lens files in conf path too, in case
    // we have not installed globally
    fs::path extra_loadpath = getConfigPath();

    if(augeas) {
        // Re-init
        aug_close(augeas);
    }

    AGO_TRACE() << "Loading Augeas with extra_loadpath="
        << extra_loadpath.string()
        << " and include path="
        << incl_path.string();

    augeas = aug_init(NULL, extra_loadpath.c_str(), AUG_SAVE_BACKUP | AUG_NO_MODL_AUTOLOAD);
    if (augeas == NULL) {
        AGO_ERROR() << "Can't initalize augeas";
        return false;
    }

    aug_set(augeas, "/augeas/load/Agocontrol/lens", "agocontrol.lns");
    aug_set(augeas, "/augeas/load/Agocontrol/incl", incl_path.c_str());
    if (aug_load(augeas) != 0) {
        // We can get errors below, even if we have 0
        AGO_ERROR() << "Augeas load ret -1";
    }

    if(aug_error(augeas)) {
        std::string err = augeasGetError();
        AGO_ERROR() << "Augeas error: " << err;

        aug_close(augeas);
        augeas = NULL;
        return false;
    }else
        AGO_TRACE() << "Augeas inited without errors";

    return true;
}

/* Internal helper to help extract errors */
static std::string augeasGetError() {
    std::vector<std::string> result;
    int err = aug_error(augeas);
    if(!err)
        return "";

    std::stringstream msg;
    const char *p;
    if((p=aug_error_message(augeas)))
        msg << p;

    if((p=aug_error_minor_message(augeas)))
        msg << " (" << p << ")";

    if((p=aug_error_details(augeas)))
        msg << " (" << p << ")";

    switch(err) {
        case AUG_EPATHX:
            return std::string("Bad section/app used: " + msg.str());
        default:
            return msg.str();
    }
}

static fs::path confPathFromApp(const std::string &app) {
    assert(!app.empty());
    return getConfigPath(MODULE_CONFDIR)
        / (app + ".conf");
}

static std::string augeasPath(const fs::path &confPath, const std::string &section, const char *option) {
    assert(!confPath.empty());
    assert(!section.empty());
    assert(option != NULL);

    std::stringstream valuepath;
    valuepath << "/files";
    valuepath << confPath.string();
    valuepath << "/";
    valuepath << section;
    valuepath << "/";
    valuepath << option;
    return valuepath.str();
}

// Default value
const ConfigNameList BLANK_CONFIG_NAME_LIST = ConfigNameList();

ConfigNameList::ConfigNameList(const char *name) {
    extra = false;
    add(name);
}

ConfigNameList::ConfigNameList(const std::string &name) {
    extra = false;
    add(name);
}

ConfigNameList::ConfigNameList(const ConfigNameList &names) {
    extra = false;
    addAll(names);
}

ConfigNameList::ConfigNameList(const ConfigNameList &names1, const ConfigNameList &names2) {
    extra = false;
    /* If any is extra, the other should be added first.
     * If neither are extra, the first with any values should be added. */
    if(names1.isExtra() && !names2.isExtra()) {
        addAll(names2);
        addAll(names1);
    }
    else if(names2.isExtra() && !names1.isExtra()) {
        addAll(names1);
        addAll(names2);
    }else{
        if(!names1.empty())
            addAll(names1);
        else if(!names2.empty())
            addAll(names2);
    }
}

ConfigNameList& ConfigNameList::add(const std::string &name) {
    name_list.push_back(name);
    return *this;
}

ConfigNameList& ConfigNameList::addAll(const ConfigNameList& names) {
    std::copy(names.name_list.begin(), names.name_list.end(),
            std::back_inserter(name_list));
    return *this;
}


std::ostream& operator<< (std::ostream& os, const ConfigNameList &list) {
    os << (list.isExtra()?"Extra":"") << "ConfigNameList[";
    BOOST_FOREACH(const std::string &name, list.names()) {
        os << name << " ";
    }
    os << "]";
    return os;
}




std::string getConfigNameListOption(const ConfigNameList &section, const char *option, const std::string &defaultValue, const ConfigNameList &app) {
    return getConfigSectionOption(section, option, (const char*)defaultValue.c_str(), app);
}

fs::path getConfigSectionOption(const ConfigNameList &section, const char *option, const fs::path &defaultValue, const ConfigNameList &app) {
    std::string value = getConfigSectionOption(section, option, (const char*)defaultValue.c_str(), app);
    return fs::path(value);
}

std::string getConfigSectionOption(const ConfigNameList &section, const char *option, const char *defaultValue, const ConfigNameList &app) {
    if (augeas==NULL){
        if(!augeas_init()) {
            return defaultValue;
        }
    }

    // If app has no values, default to section
    ConfigNameList app_(app, section);

    AGO_TRACE() << "Augeas: get " << app_ << " / " << section;
    BOOST_FOREACH(const std::string & a, app_.names()) {
        BOOST_FOREACH(const std::string & s, section.names()) {
            const char *value;
            std::string aug_path = augeasPath(confPathFromApp(a), s, option);

            int ret = aug_get(augeas, aug_path.c_str(), &value);
            if(ret == 1 && value != NULL) {
                std::stringstream result;
                AGO_TRACE() << "Augeas: using config value from " << aug_path << ": " << value;
                result << value;
                return result.str();
            }

            AGO_TRACE() << "Augeas: config value at " << aug_path << " not found (ret=" << ret << ")";
        }
    }

    if(defaultValue) {
        AGO_TRACE() << "Augeas, no match on " << section << " / " << app << ", falling back to default value " << defaultValue;
        return defaultValue;
    }

    // empty
    return std::string();
}

bool setConfigSectionOption(const char* section, const char* option, const float value, const char *app) {
    std::stringstream stringvalue;
    stringvalue << value;
    return setConfigSectionOption(section, option, stringvalue.str().c_str());
}

bool setConfigSectionOption(const char* section, const char* option, const int value, const char *app) {
    std::stringstream stringvalue;
    stringvalue << value;
    return setConfigSectionOption(section, option, stringvalue.str().c_str());
}

bool setConfigSectionOption(const char* section, const char* option, const bool value, const char *app) {
    std::stringstream stringvalue;
    stringvalue << value;
    return setConfigSectionOption(section, option, stringvalue.str().c_str());
}




bool setConfigSectionOption(const char* section, const char* option, const char* value, const char *app) {
    if (augeas==NULL){
        if(!augeas_init()) {
            AGO_ERROR() << "Save failed: augeas not inited";
            return false;
        }
    }

    if(app == NULL || !strlen(app))
        app = section;

    fs::path file = confPathFromApp(app);

    // Make file is writeable; augeas 1.2.0 (probably others) segfaults,
    // we protect ourselfs from this:
    //   https://github.com/hercules-team/augeas/issues/178
    // When the issue is fixed, we can remove this hack
    if(fs::exists(file)) {
        if(access(file.c_str(), W_OK) != 0) {
            AGO_ERROR() << "Could not write config file "
                << file.string() << ": "
                << strerror(errno);
            return false;
        }
    }

    std::string aug_path = augeasPath(file, section, option);

    if (aug_set(augeas, aug_path.c_str(), value) == -1) {
        AGO_WARNING() << "Could not set value!";
        if(aug_error(augeas)) {
            std::string err = augeasGetError();
            AGO_WARNING() << "Augeas error: " << err;
        }
        return false;
    }

    if (aug_save(augeas) == -1) {
        AGO_WARNING() << "Could not write config file " << file.string();

        if(aug_error(augeas)) {
            std::string err = augeasGetError();
            AGO_ERROR() << "Augeas error: " << err;
        }

        return false;
    }

    return true;
}

qpid::types::Variant::Map getConfigTree() {
    qpid::types::Variant::Map tree;
    if (augeas==NULL) augeas_init();
    if (augeas == NULL) {
        AGO_ERROR() << "cannot initialize augeas";
        return tree;
    }
    char **matches;
    std::stringstream path;
    path << "/files";
    path << getConfigPath(MODULE_CONFDIR).string();
    path << "/";
    std::string prefix = path.str();
    path << "/*";
    int num = aug_match(augeas, path.str().c_str(), &matches);
    for (int i=0; i < num; i++) {
        const char *val;
        aug_get(augeas, matches[i], &val);
        if (val != NULL) {
            // TODO: split augeas path into section and option
            std::string match = matches[i];
            replaceString(match, prefix, "");
            size_t pos = match.find(".conf");
            std::string section = match.substr(0,pos);
            replaceString(match, section, "");
            replaceString(match, ".conf/", "");
            std::string option = match.substr(1); // skip trailing slash
            if (!(tree[section].isVoid())) {
                qpid::types::Variant::Map sectionMap = tree[section].asMap();
                sectionMap[option] = val;
                tree[section] = sectionMap;
            } else {
                qpid::types::Variant::Map sectionMap;
                sectionMap[option] = val;
                tree[section] = sectionMap;
            }
        }
        free((void *) matches[i]);
    }
    free(matches);
    return tree;

}


}
