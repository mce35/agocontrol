/**
 * Model class
 * 
 * @returns {securityConfig}
 */
function securityConfig() {
    this.hasNavigation = ko.observable(true);
    this.devices = ko.observableArray([]);
    this.housemodes = ko.observableArray([]);
    this.zones = ko.observableArray([]);
    this.debug = ko.observable("");

    this.securityController = null;

    var self = this;

    this.devices.subscribe(function() {
	var list = self.devices().filter(function(dev) {
	    return dev.devicetype == "securitycontroller";
	});

	if (list.length > 0) {
	    self.securityController = list[0];
	}

	var content = {};
	content.command = "getzones";
	content.uuid = self.securityController.uuid;
	console.log(content);
	sendCommand(content, function(x) {
	    var before = self.debug();
	    self.debug(before + "\n getzones:" + JSON.stringify(x, undefined, 2));
	});
	
	var content = {};
	content.command = "setzones";
	content.uuid = self.securityController.uuid;
	content.zonemap = {"housemode":"armed","zones":{"armed":[{"zone":"hull","delay":12}]}};
	sendCommand(content, function(x) {
	    var before = self.debug();
	    self.debug(before + "\n setzones:" + JSON.stringify(x, undefined, 2));
	});
	
    });

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