/**
 * Model class
 * 
 * @returns {securityConfig}
 */
function securityConfig() {
    this.hasNavigation = ko.observable(true);
    this.devices = ko.observableArray([]);

    this.zoneMap = ko.observable([]);

    this.housemode = ko.observable("");
    this.housemodeNames = ko.observableArray([]);

    this.housemodes = ko.observableArray([]);
    this.zones = ko.observableArray([]);

    this.securityController = null;

    var self = this;

    /* Possible delays, we use a drop down so use a list */
    this.possibleDelays = ko.observableArray([]);
    this.possibleDelays.push("inactive");
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
     * Gets the currently active housemode
     */
    this.getHouseMode = function() {
        var content = {};
        content.command = "gethousemode";
        content.uuid = self.securityController.uuid;
        sendCommand(content, function(res) {
            if (res.result.result == 0 && res.result.housemode) {
                self.housemode(res.result.housemode);
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
        self.getHouseMode();
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

        // Housemode names
        var modeNames = [];

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

        /* Build a housemode list with delays, "inactive" means not set */
        for ( var mode in zoneMap) {
            var delays = [];
            for ( var i = 0; i < zones.length; i++) {
                delays[i] = "inactive";
            }
            for ( var i = 0; i < zoneMap[mode].length; i++) {
                delays[zoneIdx[zoneMap[mode][i].zone]] = zoneMap[mode][i].delay;
            }
            modes.push({
                name : mode,
                delays : delays
            });
            modeNames.push(mode);
        }

        self.zones(zones);
        self.housemodes(modes);
        self.housemodeNames(modeNames);
    });

    /**
     * Adds a new housemode
     */
    this.addHouseMode = function() {
        var name = $("#modeName").val();

        if ($.trim(name) == "") {
            notif.error("#emptyMode");
            return;
        }

        var modes = self.housemodes();
        self.housemodes([]);
        var delays = [];
        if (modes.length > 0) {
            for ( var i = 0; i < modes[0].delays.length; i++) {
                delays.push("inactive");
            }
        }
        modes.push({
            name : name,
            delays : delays
        });
        self.housemodes(modes);
        $("#modeName").val("");
    };

    /**
     * Adds a new zone
     */
    this.addZone = function() {
        var name = $("#zoneName").val();

        if (self.housemodes().length == 0) {
            notif.error("#modeFirst");
            return;
        }

        if ($.trim(name) == "") {
            notif.error("#emptyZone");
            return;
        }

        self.zones.push({
            name : name
        });
        var modes = self.housemodes();
        self.housemodes([]);
        for ( var i = 0; i < modes.length; i++) {
            modes[i].delays.push("inactive");
        }
        self.housemodes(modes);
        $("#zoneName").val("");
    };

    /**
     * Saves the current zone matrix
     */
    this.save = function() {
        // Current zone map
        var zoneMap = self.zoneMap();
        var zones = self.zones();

        // Index to zone map
        var idx2zone = [];

        $('.zoneKey').each(function(idx, e) {
            idx2zone[idx] = $(e).text();
        });

        var newMap = {};
        var delayList = [];

        var numDelays = self.housemodes()[0].delays.length;

        $('.delay').each(function(idx, e) {
            delayList.push($(e).val());
        });

        $('.housemode').each(function(idx, e) {
            var list = [];
            for ( var i = 0; i < numDelays; i++) {
                if (delayList[i] != "inactive") {
                    list.push({
                        delay : parseInt(delayList[i]),
                        zone : idx2zone[i]
                    });
                }
            }
            delayList = delayList.slice(numDelays);
            newMap[$(e).text()] = list;
        });

        var pin = window.prompt("PIN:");
        if (pin) {
            var content = {};
            content.command = "setzones";
            content.uuid = self.securityController.uuid;
            content.zonemap = newMap;
            content.pin = pin;
            sendCommand(content, function(res) {
                if (res.result.error) {
                    notif.error(res.result.error);
                    return;
                }
                self.getZones();
            });
        }

    };

    /**
     * Changes the current house mode this requires the user to enter the pin
     */
    this.changeHouseMode = function() {
        var content = {};
        var pin = window.prompt("PIN:");
        if (pin) {
            content.command = "sethousemode";
            content.uuid = self.securityController.uuid;
            content.mode = $('#selectedMode').val();
            content.pin = pin;
            sendCommand(content, function(res) {
                if (res.result.error) {
                    notif.error(res.result.error);
                } else {
                    self.getHouseMode();
                }
            });
        }
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
