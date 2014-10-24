#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include "agoapp.h"

#ifndef SCENARIOMAPFILE
#define SCENARIOMAPFILE "maps/scenariomap.json"
#endif

using namespace std;
using namespace agocontrol;

class AgoScenario: public AgoApp {
private:
	qpid::types::Variant::Map scenariomap;
	qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
	void setupApp();
	void runscenario(qpid::types::Variant::Map &scenario) ;

public:
	AGOAPP_CONSTRUCTOR(AgoScenario);
};

void AgoScenario::runscenario(qpid::types::Variant::Map &scenario) {
	AGO_DEBUG() << "Executing scenario";

	// build sorted list of scenario elements
	std::list<int> elements;
	for (qpid::types::Variant::Map::const_iterator it = scenario.begin(); it!= scenario.end(); it++) {
		// AGO_TRACE() << it->first;
		// AGO_TRACE() << it->second;
		elements.push_back(atoi(it->first.c_str()));
	}
	// AGO_TRACE() << "elements: " << elements;
	elements.sort();
	for (std::list<int>::const_iterator it = elements.begin(); it != elements.end(); it++) {
		// AGO_TRACE() << *it;
		int seq = *it;
		stringstream sseq;
		sseq << seq;
		qpid::types::Variant::Map element = scenario[sseq.str()].asMap();
		AGO_TRACE() << sseq.str() << ": " << scenario[sseq.str()];
		if (element["command"] == "scenariosleep") {
			try {
				int delay = element["delay"];
				AGO_DEBUG() << "scenariosleep special command detected. Delay: " << delay;
				sleep(delay);
			} catch (qpid::types::InvalidConversion) {
				AGO_ERROR() << "Invalid conversion of delay value";
			}
		} else {
			AGO_DEBUG() << "sending scenario command: " << element;
			agoConnection->sendMessage(element);
		}
	}
}

qpid::types::Variant::Map AgoScenario::commandHandler(qpid::types::Variant::Map content) {
	qpid::types::Variant::Map returnval;
	std::string internalid = content["internalid"].asString();
	if (internalid == "scenariocontroller") {
		if (content["command"] == "setscenario") {
			try {
				AGO_TRACE() << "setscenario request";
				qpid::types::Variant::Map newscenario = content["scenariomap"].asMap();
				AGO_TRACE() << "scnario content:" << newscenario;
				std::string scenariouuid = content["scenario"].asString();
				if (scenariouuid == "") scenariouuid = generateUuid();
				AGO_TRACE() << "scenario uuid:" << scenariouuid;
				scenariomap[scenariouuid] = newscenario;
				agoConnection->addDevice(scenariouuid.c_str(), "scenario", true);
				if (variantMapToJSONFile(scenariomap, getConfigPath(SCENARIOMAPFILE))) {
					returnval["result"] = 0;
					returnval["scenario"] = scenariouuid;
				} else {
					returnval["result"] = -1;
				}
			} catch (qpid::types::InvalidConversion) {
				returnval["result"] = -1;
			} catch (...) {
				returnval["result"] = -1;
				returnval["error"] = "exception";
			}
		} else if (content["command"] == "getscenario") {
			try {
				std::string scenario = content["scenario"].asString();
				AGO_TRACE() << "getscenario request:" << scenario;
				returnval["result"] = 0;
				returnval["scenariomap"] = scenariomap[scenario].asMap();
				returnval["scenario"] = scenario;
			} catch (qpid::types::InvalidConversion) {
				returnval["result"] = -1;
			}
		} else if (content["command"] == "delscenario") {
			std::string scenario = content["scenario"].asString();
			AGO_TRACE() << "delscenario request:" << scenario;
			returnval["result"] = -1;
			if (scenario != "") {
				qpid::types::Variant::Map::iterator it = scenariomap.find(scenario);
				if (it != scenariomap.end()) {
					AGO_DEBUG() << "removing ago device" << scenario;
					agoConnection->removeDevice(it->first.c_str());
					scenariomap.erase(it);
					if (variantMapToJSONFile(scenariomap, getConfigPath(SCENARIOMAPFILE))) {
						returnval["result"] = 0;
					}
				}
			}
		} 

	} else {

		if ((content["command"] == "on") || (content["command"] == "run")) {
			AGO_DEBUG() << "spawning thread for scenario: " << internalid;
			boost::thread t(boost::bind(&AgoScenario::runscenario, this, scenariomap[internalid].asMap()));
			t.detach();
			returnval["result"] = 0;
		} 

	}
	return returnval;
}

void AgoScenario::setupApp() {
	addCommandHandler();
	agoConnection->addDevice("scenariocontroller", "scenariocontroller");

	scenariomap = jsonFileToVariantMap(getConfigPath(SCENARIOMAPFILE));
	for (qpid::types::Variant::Map::const_iterator it = scenariomap.begin(); it!=scenariomap.end(); it++) {
		AGO_DEBUG() << "Loading scenario: " << it->first << ":" << it->second;
		agoConnection->addDevice(it->first.c_str(), "scenario", true);
	}
}

AGOAPP_ENTRY_POINT(AgoScenario);
