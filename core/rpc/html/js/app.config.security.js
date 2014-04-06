/**
 * Model class
 * 
 * @returns {securityConfig}
 */
function securityConfig() {
    this.hasNavigation = ko.observable(true);
    this.devices = ko.observableArray([]);

    this.zoneMap = ko.observable([]);

    this.housemodes = ko.observableArray([]);
    this.zones = ko.observableArray([]);

    this.securityController = null;

    var self = this;

    /* Possible delays, we use a drop down so use a list */
    this.possibleDelays = ko.observableArray([]);
    this.possibleDelays.push(-1);
    this.possibleDelays.push(0);
    for ( var i = 1; i <= 256; i *= 2) {
	this.possibleDelays.push(i);
    }

    /**
     * Get the zones and place into a map
     */
    this.getZones = function() {
	var content = {};
	content.command = "getzones";
	content.uuid = self.securityController.uuid;
	sendCommand(content, function(res) {
	    if (res.result.result == 0 && res.result.zonemap) {
		self.zoneMap(res.result.zonemap);
	    }
	});
    };

    /**
     * Wait for the securityController and build the zone map
     */
    this.devices.subscribe(function() {
	var list = self.devices().filter(function(dev) {
	    return dev.devicetype == "securitycontroller";
	});

	if (list.length > 0) {
	    self.securityController = list[0];
	}

	self.getZones();
    });

    this.zoneMap.subscribe(function() {
	// Current zone map
	var zoneMap = self.zoneMap();

	// Temporary housemode liste
	var modes = [];
	// List of zone names
	var zones = [];
	// Zone names to index mapping
	var zoneIdx = {};

	/* Build list of zones and index mapping */
	for ( var mode in zoneMap) {
	    for ( var i = 0; i < zoneMap[mode].length; i++) {
		if (zoneIdx[zoneMap[mode][i].zone] === undefined) {
		    zones.push({
			name : zoneMap[mode][i].zone
		    });
		    zoneIdx[zoneMap[mode][i].zone] = i;
		}
	    }
	}

	/* Build a housemode list with delays, -1 means not set */
	for ( var mode in zoneMap) {
	    var delays = [];
	    for ( var i = 0; i < zones.length; i++) {
		delays[i] = "-1";
	    }
	    for ( var i = 0; i < zoneMap[mode].length; i++) {
		delays[zoneIdx[zoneMap[mode][i].zone]] = zoneMap[mode][i].delay;
	    }
	    modes.push({
		name : mode,
		delays : delays
	    });
	}

	self.zones(zones);
	self.housemodes(modes);
    });

    /***
     * Just a test
     * XXX: Remove
     */
    this.testTrigger = function() {
	var content = {};
	content.command = "sethousemode";
	content.uuid = self.securityController.uuid;
	content.mode = "armed";
	content.pin = "0815";
	sendCommand(content, function() {
	    var content = {};
	    content.command = "triggerzone";
	    content.uuid = self.securityController.uuid;
	    content.zone = "hull";
	    sendCommand(content, function(x) {
		console.log(x);
	    });
	});
    };

    /**
     * Adds a new housemode
     */
    this.addHouseMode = function() {
	var name = $("#modeName").val();
	var modes = self.housemodes();
	self.housemodes([]);
	var delays = [];
	if (modes.length > 0) {
	    for ( var i = 0; i < modes[0].delays.length; i++) {
		delays.push("-1");
	    }
	}
	modes.push({
	    name : name,
	    delays : delays
	});
	self.housemodes(modes);
    };

    /**
     * Adds a new zone
     */
    this.addZone = function() {
	var name = $("#zoneName").val();
	self.zones.push({
	    name : name
	});
	var modes = self.housemodes();
	self.housemodes([]);
	for ( var i = 0; i < modes.length; i++) {
	    console.log(modes[i].delays);
	    modes[i].delays.push("-1");
	    console.log(modes[i].delays);
	}
	self.housemodes(modes);
    };

    /**
     * Saves the current zone matrix
     */
    this.save = function() {
	// Current zone map
	var zoneMap = self.zoneMap();
	var zones = self.zones();

	// Zone names to index mapping
	var zoneIdx = {};
	var idx2zone = [];

	/* Build list of zones and index mapping */
	for ( var mode in zoneMap) {
	    for ( var i = 0; i < zoneMap[mode].length; i++) {
		if (zoneIdx[zoneMap[mode][i].zone] === undefined) {
		    zones.push({
			name : zoneMap[mode][i].zone
		    });
		    zoneIdx[zoneMap[mode][i].zone] = i;
		    idx2zone[i] = zoneMap[mode][i].zone;
		}
	    }
	}

	var newMap = {};
	var delayList = [];

	var numDelays = self.housemodes()[0].delays.length;

	$('.delay').each(function(idx, e) {
	    delayList.push($(e).val());
	});

	$('.housemode').each(function(idx, e) {
	    var list = [];
	    for ( var i = 0; i < numDelays; i++) {
		if (delayList[i] != -1) {
		    list.push({
			delay : delayList[i],
			zone : idx2zone[i]
		    });
		}
	    }
	    delayList = delayList.slice(numDelays);
	    newMap[$(e).text()] = list;
	});

	var content = {};
	content.command = "setzones";
	content.uuid = self.securityController.uuid;
	content.zonemap = newMap;
	sendCommand(content, function(x) {
	    self.getZones();
	});

    };

}
/**
 * Initalizes the model
 */
function init_securityConfig() {
    model = new securityConfig();

    model.mainTemplate = function() {
	return "configuration/security";
    }.bind(model);

    model.navigation = function() {
	return "navigation/configuration";
    }.bind(model);

    ko.applyBindings(model);

}