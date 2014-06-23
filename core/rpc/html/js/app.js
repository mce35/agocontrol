infuser.defaults.templateUrl = "/templates";

Array.prototype.chunk = function(chunkSize) {
    var array = this;
    return [].concat.apply([], array.map(function(elem, i) {
	return i % chunkSize ? [] : [ array.slice(i, i + chunkSize) ];
    }));
};

if (!Date.prototype.toISOString) {
    (function() {

	function pad(number) {
	    var r = String(number);
	    if (r.length === 1) {
		r = '0' + r;
	    }
	    return r;
	}

	Date.prototype.toISOString = function() {
	    return this.getUTCFullYear() + '-' + pad(this.getUTCMonth() + 1) + '-' + pad(this.getUTCDate()) + 'T' + pad(this.getUTCHours()) + ':' + pad(this.getUTCMinutes()) + ':'
		    + pad(this.getUTCSeconds()) + '.' + String((this.getUTCMilliseconds() / 1000).toFixed(3)).slice(2, 5) + 'Z';
	};

    }());
}

ko.bindingHandlers.slider = {
    init : function(element, valueAccessor, allBindingsAccessor) {
	var options = allBindingsAccessor().sliderOptions || {};
	$(element).slider(options);
	ko.utils.registerEventHandler(element, "slidechange", function(event, ui) {
	    var observable = valueAccessor();
	    observable(ui.value);
	    // Hack to avoid setting the level on startup
	    // So we call the syncLevel method when we have
	    // a mouse event (means user triggered).
	    if (options.dev && event.clientX) {
		options.dev.syncLevel();
	    }
	});
	ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
	    $(element).slider("destroy");
	});
    },
    update : function(element, valueAccessor) {
	var value = valueAccessor();
	if (isNaN(value))
	    value = 0;
	$(element).slider("value", value);
    }
};

/* Enable caching for ajax requests */
$.ajaxSetup({
    cache : true
});

function getPage() {
    var query = window.location.search.substring(1);
    query = query.split("&")[0];
    return query ? query : "dashboard";
}

var subscription = null;
var url = "/jsonrpc";

var schema = {};
var deviceMap = [];
var rooms = {};
var floorPlans = {};
var pluginNames = {};
var systemvar = {};
var variables = {};
var environment = {};
var inventory = ko.observable({});
var currentFloorPlan = ko.observable({});

var agoController = null;
var eventController = null;
var dataLoggerController = null;
var scenarioController = null;
var alertControler = null;
var rrdtoolController = null;
var model = null;
var initialized = false;

var supported_devices = [];

function buildfloorPlanList(model) {
    model.floorPlans = ko.observableArray([]);
    for ( var k in floorPlans) {
	model.floorPlans.push({
	    uuid : k,
	    name : floorPlans[k].name,
	    action : '', // dummy for table

	    /* Called from link onClick in navigation/main.html */
	    showFloorplan : function(data, event) {
		if (getPage() !== 'floorplan')
		    return true; // Let href act instead; no support for
		// reloading page-script

		setFloorPlan(this.uuid);
		return false;
	    }
	});
    }
}

function buildPluginNamesList(model) {
    model.pluginNames = ko.observableArray([]);
    for ( var i=0; i<pluginNames.length; i++ ) {
        //filter plugins that are not accessible from dashboard (using dropdown button)
        if( pluginNames[i].dashboardAccess===undefined || pluginNames[i].dashboardAccess==true ) {
            model.pluginNames.push({
                name : pluginNames[i].name,
                _name : pluginNames[i]._name
            });
        }
    }
}
/**
 * This gets set when the GUI needs to be initalized after loading the
 * inventory.
 */
var deferredInit = null;

function loadPlugin(fromDashboard) {
    // lock uia
    $.blockUI({
        message : '<div>Please wait ...</div>',
        css : {
            border : '3px solid #a00'
        }
    });

    /* Get plugin name from query string */
    var name = window.location.search.substring(1);
    var tmp = name.split("&");
    for ( var i = 0; i < tmp.length; i++) {
        if (tmp[i].indexOf("name=") == 0) {
            name = tmp[i].split("=")[1];
        }
    }
    name = name.replace(/\//g, "");
    $.getScript("plugins/" + name + "/plugin.js", function() {
    $.ajax({
	    url : "/cgi-bin/pluginlist.cgi",
        method : "GET",
        async : true,
    }).done(function(result) {
        var plugin = result.filter(function(p) {
            return p._name.toLowerCase() == name.toLowerCase();
        })[0];
        templatePath = "../plugins/" + name + "/templates/";
        /* Load the plugins resources if any */
        if (plugin.resources) {
            var resources = [];
            if (plugin.resources.css && plugin.resources.css.length > 0) {
                for ( var i = 0; i < plugin.resources.css.length; i++) {
                    resources.push("plugins/" + name + "/" + plugin.resources.css[i]);
                }
            }
            if (plugin.resources.js && plugin.resources.js.length > 0) {
                for ( var i = 0; i < plugin.resources.js.length; i++) {
                    resources.push("plugins/" + name + "/" + plugin.resources.js[i]);
                }
            }
            if (resources.length > 0) {
                yepnope({
                    load : resources,
                    complete : function() {
                        // here, all resources are really loaded
                        init_plugin(fromDashboard);
                        // unlock ui
                        $.unblockUI();
                    }
                });
            }
        } else {
            $.unblockUI();
            init_plugin(fromDashboard);
        }
    });
    }).fail(function() {
        $.unblockUI();
        notif.fatal("Error: Failed to load plugin!");
    });
}
function loadPluginDashboard() {
    loadPlugin(true);
}

function getPluginNames() {  
    $.ajax({
        url : "/cgi-bin/pluginlist.cgi",
        method : "GET",
        async : false,
    }).done(function(result) {
        pluginNames = result;
    });
}

function initGUI() {
    var page = getPage();
    if (page == "dashboard") {
	//load plugin names only in dashboard
	getPluginNames();
	deferredInit = init_dashBoard;
    } else if (page == "floorplan") {
	deferredInit = init_floorPlan;
    } else if (page == "roomConfig") {
	init_roomConfig();
    } else if (page == "variablesConfig") {
	init_variablesConfig();
    } else if (page == "floorplanConfig") {
	init_floorplanConfig();
    } else if (page == "configuration") {
	deferredInit = init_configuration;
    } else if (page == "cloudConfig") {
	deferredInit = init_cloudConfig;
    } else if (page == "deviceConfig") {
	init_deviceConfig();
    } else if (page == "systemConfig") {
	deferredInit = init_systemConfig;
    } else if (page == "eventConfig") {
	init_eventConfig();
    } else if (page == "scenarioConfig") {
	init_scenarioConfig();
    } else if (page == "securityConfig") {
	init_securityConfig();
    } else if (page == "inventoryView") {
	deferredInit = init_inventoryView;
    } else if (page == "systemStatus") {
	init_systemStatus();
    } else if (page == "pluginsConfig") {
	deferredInit = init_pluginsConfig;
    } else if (page == "plugin") {
	deferredInit = loadPlugin;
    } else if (page == "pluginDashboard") {
	deferredInit = loadPluginDashboard;
    }

    /* Parse floorplan uuid */
    var fp = window.location.search.substring(1);
    var tmp = fp.split("&");
    for ( var i = 0; i < tmp.length; i++) {
	if (tmp[i].indexOf("fp=") == 0) {
	    fp = tmp[i].split("=")[1];
	}
    }

    // When inventory is ready, assign initial floorplan
    if (!fp)
	return;

    var obs = inventory.subscribe(function(inv) {
	// Never push state on page-load
	setFloorPlan(fp, true);
	obs.dispose();
    });
}

function getStartJSFile() {
    var page = getPage();

    if (page == "dashboard") {
	return "js/app.dashboard.js";
    } else if (page == "floorplan") {
	return "js/app.floorplan.js";
    } else if (page == "roomConfig") {
	return "js/app.config.rooms.js";
    } else if (page == "variablesConfig") {
	return "js/app.config.variables.js";
    } else if (page == "floorplanConfig") {
	return "js/app.config.floorplan.js";
    } else if (page == "configuration") {
	return "js/app.configuration.js";
    } else if (page == "cloudConfig") {
	return "js/app.config.cloud.js";
    } else if (page == "deviceConfig") {
	return "js/app.config.devices.js";
    } else if (page == "systemConfig") {
	return "js/app.config.system.js";
    } else if (page == "eventConfig") {
	return "js/app.config.events.js";
    } else if (page == "scenarioConfig") {
	return "js/app.config.scenarios.js";
    } else if (page == "inventoryView") {
	return "js/app.inventory.js";
    } else if (page == "systemStatus") {
	return "js/app.systemstatus.js";
    } else if (page == "pluginsConfig") {
	return "js/app.config.plugins.js";
    } else if (page == "securityConfig") {
	return "js/app.config.security.js";
    }

    return null;
}

/**
 * Loads the userinterface and initates the subscription
 * 
 * @param msg
 */
function loadInterface(msg) {
    supported_devices = msg;
    sessionStorage.supported_devices = JSON.stringify(msg);
    var startfile = getStartJSFile();
    if (startfile !== null) {
	$.getScript(startfile, function() {
	    initGUI();
	    if (localStorage.inventoryCache) {
		handleInventory(null);
	    }
	    subscribe();
	});
    } else {
	initGUI();
	if (localStorage.inventoryCache) {
	    handleInventory(null);
	}
	subscribe();
    }
}

/* Load the device template names before loading the gui */
if (sessionStorage.supported_devices) {
    loadInterface(JSON.parse(sessionStorage.supported_devices));
} else {
    $.ajax({
	url : "/cgi-bin/listing.cgi?devices=1",
	type : "GET"
    }).done(loadInterface);
}

// --- AGO --- //

var securityPromted = false;

function handleEvent(response) {
    if (response.result.event == "event.security.countdown" && !securityPromted) {
        securityPromted = true;
        var pin = window.prompt("Alarm please entry pin:");
        var content = {};
        content.command = "cancel";
        content.uuid = response.result.uuid;
        content.pin = pin;
        sendCommand(content, function(res) {
            if (res.result.error) {
                notif.error(res.result.error);
            }
            securityPromted = false;
        });
        return;
    } else if (response.result.event == "event.security.intruderalert") {
        notif.error("INTRODUCER ALERT!");
        return;
    }
    for ( var i = 0; i < deviceMap.length; i++) {
    	if (deviceMap[i].uuid == response.result.uuid ) {
            // update device last seen datetime
            deviceMap[i].timeStamp(formatDate(new Date()));
            //update device level
            if( response.result.level !== undefined) {
                // update custom device member
                if (response.result.event.indexOf('event.device') != -1 && response.result.event.indexOf('changed') != -1) {
                    // event that update device member
                    var member = response.result.event.replace('event.device.', '').replace('changed', '');
                    if (deviceMap[i][member] !== undefined) {
                        deviceMap[i][member](response.result.level);
                    }
                }
                // Binary sensor has its own event
                else if (response.result.event == "event.security.sensortriggered") {
                    if (deviceMap[i]['state'] !== undefined) {
                        deviceMap[i]['state'](response.result.level);
                    }
                }
            }
            //update quantity
            if (response.result.quantity) {
                var values = deviceMap[i].values();
                /* We have no values so reload from inventory */
                if (values[response.result.quantity] === undefined) {
                    getInventory(function(inv) {
                        var tmpInv = cleanInventory(inv.result.devices);
                        if (tmpInv[response.result.uuid] !== undefined) {
                            if (tmpInv[response.result.uuid].values) {
                                deviceMap[i].values(tmpInv[response.result.uuid].values);
                            }
                        }
                    });
                    break;
                }
                if( response.result.level !== undefined ) {
                    if( response.result.quantity==='forecast' && typeof response.result.level=="string" )
                    {
                        //update forecast value for barometer sensor only if string specified
                        deviceMap[i].forecast(response.result.level);
                    }
                    //save new level
                    values[response.result.quantity].level = response.result.level;
                }
                else if( response.result.latitude!==undefined && response.result.longitude!==undefined ) {
                    values[response.result.quantity].latitude = response.result.latitude;
                    values[response.result.quantity].longitude = response.result.longitude;
                }
                deviceMap[i].values(values);
            }
            break;
        }
    }
    getEvent();
}

function getEvent() {
    var request = {};
    request.method = "getevent";
    request.params = {};
    request.params.uuid = subscription;
    request.id = 1;
    request.jsonrpc = "2.0";

    $.post(url, JSON.stringify(request), handleEvent, "json");
}

function cleanInventory(data) {
    for ( var k in data) {
	if (!data[k]) {
	    delete data[k];
	}
    }

    return data;
}

function handleInventory(response) {
    if (response != null && response.result.match !== undefined && response.result.match(/^exception/)) {
	notif.error("RPC ERROR: " + response.result);
	return;
    }

    if (response == null) {
	response = {
	    result : JSON.parse(localStorage.inventoryCache)
	};
    } else {
	localStorage.inventoryCache = JSON.stringify(response.result);
    }

    rooms = response.result.rooms;
    systemvar = response.result.system;
    schema = response.result.schema;
    floorPlans = response.result.floorplans;
    variables = response.result.variables;
    environment = response.result.environment;

    var inv = cleanInventory(response.result.devices);
    var found;
    for ( var uuid in inv) {
	if (inv[uuid].room !== undefined && inv[uuid].room) {
	    inv[uuid].roomUID = inv[uuid].room;
	    if (rooms[inv[uuid].room] !== undefined) {
		inv[uuid].room = rooms[inv[uuid].room].name;
	    } else {
		inv[uuid].room = "";
	    }

	} else {
	    inv[uuid].room = "";
	}

	found = false;
	for ( var i = 0; i < deviceMap.length; i++) {
	    if (deviceMap[i].uuid === uuid) {
		// device already exists in deviceMap array. Update its content
		deviceMap[i].update(inv[uuid], uuid);
		found = true;
		break;
	    }
	}
	if (!found) {
	    deviceMap.push(new device(inv[uuid], uuid));
	}
    }

    // Notify global observable about new inventory
    inventory(inv);

    if (deferredInit && !initialized) {
	deferredInit();
	initialized = true;
    }

    if (!model) {
	return;
    }

    if (model.deviceCount !== undefined) {
	if (deviceMap.length != model.deviceCount()) {
	    model.deviceCount(deviceMap.length);
	}
    }

    if (model.devices !== undefined) {
	if (JSON.stringify(deviceMap) != JSON.stringify(model.devices())) {
	    model.devices(deviceMap);
	}
    }

    if (model.inventory !== undefined) {
	if (JSON.stringify(response.result) != JSON.stringify(model.inventory())) {
	    model.inventory(response.result);
	}
    }

    if (model.rooms !== undefined && model.rooms.slice !== undefined) {
	/* get uuid into rooms */
	model.rooms([]);
	for ( var uuid in rooms) {
	    var tmp = rooms[uuid];
	    tmp.uuid = uuid;
	    tmp.action = ''; // dummy for table
	    model.rooms.push(tmp);
	}
    }

    if (model.floorplans !== undefined) {
	/* get uuid into floorplans */
	var fpList = [];
	for ( var uuid in floorPlans) {
	    var tmp = floorPlans[uuid];
	    tmp.uuid = uuid;
	    tmp.action = ''; // dummy for table
	    fpList.push(tmp);
	}

	if (JSON.stringify(fpList) != JSON.stringify(model.floorplans())) {
	    model.floorplans(fpList);
	}
    }

    if (model.variables !== undefined) {
	/* build variable pairs */
	var varList = [];
	for ( var key in variables) {
	    var tmp = {};
	    tmp.variable = key;
	    tmp.value = variables[key];
	    tmp.action = ''; // dummy for table
	    varList.push(tmp);
	}

	if (JSON.stringify(varList) != JSON.stringify(model.variables())) {
	    model.variables(varList);
	}
    }
}

/**
 * Lookup floorplan by uuid, and assign currentFloorPlan observable. Also tries
 * to set HTML5 history state object for re-navigation.
 * 
 * Parameter replaceState can have the following values: - undefined/false to
 * use pushState (new page navigation) - true to use replaceState (fresh page
 * load, where we just want to assign a state object to the current view) - null
 * to not do any state management (i.e. after called from onpopstate)
 */
function setFloorPlan(fp, replaceState) {
    // No change?
    if (currentFloorPlan().uuid === fp)
	return;

    for ( var k in floorPlans) {
	if (k == fp) {
	    var tmp;
	    tmp = floorPlans[k];
	    tmp.uuid = k;
	    currentFloorPlan(tmp);
	    break;
	}
    }

    if (replaceState === null)
	return;

    // Not a floor plan page so don't do anything
    if (getPage() != "floorplan") {
	return;
    }

    // HTML5 push/pop-state support
    try {
	if (history && history.pushState && history.replaceState) {
	    var fn = replaceState ? history.replaceState : history.pushState;
	    fn.apply(history, [ {
		page : 'floorplan',
		fp : fp
	    }, document.title, '?floorplan&fp=' + fp ]);
	}
    } catch (e) {
	console.error(e);
    }
}

/*
 * If HTML5 history state is available, this will trigger when user navigates
 * back/forth. If a history.pushState event has been executed, we can find out
 * what page to render. Currently only supports floorplan
 */
window.onpopstate = function(event) {
    if (event.state && event.state.page === 'floorplan') {
	setFloorPlan(event.state.fp, null);
    }
};

function getInventory(customCb) {
    var cb = customCb || handleInventory;
    var content = {};
    content.command = "inventory";
    sendCommand(content, cb);
}

function unsubscribe() {
    var request = {};
    request.method = "unsubscribe";
    request.id = 1;
    request.jsonrpc = "2.0";
    request.params = {};
    request.params.uuid = subscription;

    $.post(url, JSON.stringify(request), function() {
    }, "json");
}

function handleSubscribe(response) {
    if (response.result) {
	subscription = response.result;
	getInventory();
	getEvent();
	window.onbeforeunload = function(event) {
	    unsubscribe();
	};
    }
}

function sendCommand(content, callback, timeout) {
    var request = {};
    request.method = "message";
    request.params = {};
    request.params.content = content;
    if (timeout) {
	request.params.replytimeout = timeout;
    }
    request.id = 1;
    request.jsonrpc = "2.0";

    $.ajax({
	type : 'POST',
	url : url,
	data : JSON.stringify(request),
	success : function(r) {
	    if (callback !== undefined) {
		callback(r);
	    }
	},
	dataType : "json",
	async : true
    });
}

function subscribe() {
    var request = {};
    request.method = "subscribe";
    request.id = 1;
    request.jsonrpc = "2.0";

    $.post(url, JSON.stringify(request), handleSubscribe, "json");
}

$(function() {
    $("#colorPickerDialog").dialog({
	autoOpen : false,
	modal : true,
	minHeight : 300,
	minWidth : 300,
	buttons : {
	    Cancel : function() {
		$(this).dialog("close");
	    },
	    OK : function() {
		var content = {};
		content.uuid = $("#colorPickerDialog").data('uuid');
		content.command = "setcolor";
		var color = $('#colorValue').val();
		content.red = ~~(parseInt(color.substring(0, 2), 16) * 100 / 255);
		content.green = ~~(parseInt(color.substring(2, 4), 16) * 100 / 255);
		content.blue = ~~(parseInt(color.substring(4, 6), 16) * 100 / 255);
		sendCommand(content);
		$(this).dialog("close");
	    }
	}
    });
});

/* Opens color picker */
function openColorPicker(uuid) {
    $("#colorPickerDialog").data('uuid', uuid);
    $("#colorPickerDialog").dialog("open");
}

/**
 * Opens details page for the given device
 * 
 * @param device
 * @param environment
 */
function showDetails(device, environment) {
    /* Check if we have a template if yes use it otherwise fall back to default */
    $.ajax({
	type : 'HEAD',
	url : "templates/details/" + device.devicetype + ".html",
	success : function() {
	    doShowDetails(device, device.devicetype, environment);
	},
	error : function() {
	    doShowDetails(device, "default");
	}
    });
}

/**
 * Formats a date object
 * 
 * @param date
 * @param simple
 * @returns {String}
 */
function formatDate(date) {
    return date.toLocaleDateString() + " " + date.toLocaleTimeString();
};

/**
 * Shows the command selector for the detail pages
 * 
 * @param container
 * @param device
 */
function showCommandList(container, device) {
    var commandSelect = document.createElement("select");
    var commandParams = document.createElement("span");
    commandSelect.id = "commandSelect";
    var type = device.devicetype;
    if( type && schema.devicetypes[type] ) {
        for ( var i = 0; i < schema.devicetypes[type].commands.length; i++) {
            commandSelect.options[i] = new Option(schema.commands[schema.devicetypes[type].commands[i]].name, schema.devicetypes[type].commands[i]);
        }
    }

    commandSelect.onchange = function() {
	if (commandSelect.options.length == 0) {
	    return 0;
	}
	var cmd = schema.commands[commandSelect.options[commandSelect.selectedIndex].value];
	commandParams.innerHTML = "";
	if (cmd.parameters !== undefined) {
	    commandParams.style.display = "";
	    for ( var param in cmd.parameters) {
		if (cmd.parameters[param].type == 'option') {
		    var select = document.createElement("select");
		    select.name = param;
		    select.className = "cmdParam";
		    for ( var i = 0; i < cmd.parameters[param].options.length; i++)
			select.options[select.options.length] = new Option(cmd.parameters[param].options[i], cmd.parameters[param].options[i]);
		    commandParams.appendChild(select);
		} else {
		    var input = document.createElement("input");
		    input.name = param;
		    input.className = "cmdParam";
		    input.placeholder = cmd.parameters[param].name;
		    commandParams.appendChild(input);
		}
	    }
	} else {
	    commandParams.style.display = "none";
	}
    };

    commandSelect.onchange();

    container.appendChild(commandSelect);
    container.appendChild(commandParams);
}

/**
 * Convert string under format "d.m.y h:m" to js Date object
 */
function stringToDatetime(str) {
    var dt = str.match(/(\d+).(\d+).(\d+)\s+(\d+):(\d+)/);
    return new Date(dt[3], parseInt(dt[2])-1, dt[1], dt[4], dt[5]);
}

/**
 * Convert datetime js object to string under format "d.m.y h:m"
 */
function datetimeToString(dt) {
    var str = $.datepicker.formatDate('dd.mm.yy', dt);
    str += ' ';
    str += ( dt.getHours()<10 ? '0'+dt.getHours() : dt.getHours() );
    str += ':';
    str += ( dt.getMinutes()<10 ? '0'+dt.getMinutes() : dt.getMinutes() );
    return str;
}

/**
 * Shows the detail page of a device
 * 
 * @param device
 * @param template
 * @param environment
 */
function doShowDetails(device, template, environment) {
    ko.renderTemplate("details/" + template, device, {
	afterRender : function() {
	    var dialogWidth = 800;
	    var dialogHeight = 300;

	    if (document.getElementById('commandList')) {
		showCommandList(document.getElementById('commandList'), device);
	    }

	    if (device.devicetype == "camera") {
		dialogHeight = 620;
		device.getVideoFrame();
	    }

	    if (document.getElementById('graph') && ((device.valueList && device.valueList() && device.valueList().length) || device.devicetype == "binarysensor")) {
            //refresh button
            $('#get_graph').click(function() {
                renderGraph(device, document.getElementById('graph')._environment);
            });

		/* Setup start date */
		var start = new Date((new Date()).getTime() - 24 * 3600 * 1000);
        var startEl = $('#start_date');
        startEl.val($.datepicker.formatDate('dd.mm.yy', start) + ' 00:00');
        startEl.datetimepicker({
            closeOnDateSelect: true,
            //value: $.datepicker.formatDate('dd.mm.yy', start) + ' 00:00',
            format: 'd.m.Y H:i',
            onChangeDateTime: function(dp,$input) {
                //check date
                var sd = stringToDatetime($('#start_date').val());
                var ed = stringToDatetime($('#end_date').val());
                if( sd.getTime()>ed.getTime() ) {
                    //invalid date
                    notif.warning('Specified datetime is invalid');
                    sd = new Date(ed.getTime() - 24 * 3600 * 1000);
                    $('#start_date').val( datetimeToString(sd) );
                }
            }
        });

		/* Setup end date */
        var endEl = $('#end_date');
        endEl.val($.datepicker.formatDate('dd.mm.yy', new Date())+' 23:59');
        endEl.datetimepicker({
            closeOnDateSelect: true,
            format:'d.m.Y H:i',
            onChangeDateTime: function(dp,$input) {
                //check date
                var sd = stringToDatetime($('#start_date').val());
                var ed = stringToDatetime($('#end_date').val());
                if( sd.getTime()>ed.getTime() ) {
                    //invalid date
                    notif.warning('Specified datetime is invalid');
                    ed = new Date(sd.getTime() + 24 * 3600 * 1000);
                    $('#end_date').val( datetimeToString(ed) );
                }
            }
        });

		if (device.devicetype == "binarysensor") {
		    environment = "device.state";
		}

		renderGraph(device, environment ? environment : device.valueList()[0].name);

		document.getElementsByName("displayType")[0].onchange = function() {
		    renderGraph(device, environment ? environment : device.valueList()[0].name);
		};

		document.getElementsByName("displayType")[1].onchange = function() {
		    renderGraph(device, environment ? environment : device.valueList()[0].name);
		};

		var reset = function() {
		    if (device !== undefined)
			device.reset();
		};

		dialogWidth = 1000;
		dialogHeight = 720;
	    }

	    if (document.getElementById("detailsTitle")) {
		$("#detailsPage").dialog({
		    title : document.getElementById("detailsTitle").innerHTML,
		    modal : true,
		    width : Math.min(dialogWidth, Math.round(screen.width * 0.8)),
		    height : Math.min(dialogHeight, Math.round(screen.height * 0.8)),
		    close : function() {
			var graphContainer = document.getElementById('graph');
			if (graphContainer) {
			    graphContainer.parentNode.removeChild(graphContainer);
			}
		    },
		    open : function() {
			$("#detailsPage").css("overflow", "visible");
			if (device.dialogopened !== undefined)
			    device.dialogopened(this);
		    }
		});
	    }

	}
    }, document.getElementById("detailsPage"));
}

/**
 * Renders the graph for the given device and environment
 * 
 * @param device
 * @param environment
 */
function renderGraph(device, environment) {

    var renderType = $($('input[name="displayType"]:checked')[0]).val();

    $('#graph').show();
    $('#dataList').hide();

    $('#graph').block({
	message : '<div>Please wait ...</div>',
	css : {
	    border : '3px solid #a00'
	}
    });

    var endDate = stringToDatetime($('#end_date').val());
    var startDate = stringToDatetime($('#start_date').val());

    //check if agorrdtool is installed
    if( rrdtoolController && renderType=="graph" ) {
        //fix end date
        now = new Date();
        if( endDate.getTime()>now.getTime() )
           endDate = now;
        //prepare content
        $('#graph').empty();
        var img = $('<img src="" id="graphRRD" style="display: none"/>');
        $('#graph').append(img);
        //get graph
        device.getRrdGraph(device.uuid, Math.round(startDate.getTime()/1000), Math.round(endDate.getTime()/1000));
        //do not generate other graph
        return;
    }

    var max_ticks = 25; // User option?

    var content = {};
    content.uuid = dataLoggerController;
    content.command = "getdata";
    content.deviceid = device.uuid;
    content.start = startDate.toISOString();
    content.end = endDate.toISOString();
    content.env = environment.toLowerCase();

    sendCommand(content, function(res) {
	if (!res.result || !res.result.result || !res.result.result.values) {
	    notif.fatal("Error while loading Graph!");
	    $('#graph').unblock();
	    return;
	}

	/* Get the unit */
	var unit = "";
	for ( var k = 0; k < device.valueList().length; k++) {
	    if (device.valueList()[k].name == environment) {
		unit = device.valueList()[k].unit;
		break;
	    }
	}

	/* Prepare the data */
	var data = [];
	var values = res.result.result.values;

    if (renderType == "list") {
        values.sort(function(a, b) {
            return b.time - a.time;
        });

        if( values.length>0 ) {
            if( values[0].level ) {
                for ( var i = 0; i < values.length; i++) {
                    values[i].date = formatDate(new Date(values[i].time * 1000));
                    values[i].value = values[i].level + " " + unit;
                    delete values[i].level;
                }
            }
            else if( values[0].latitude && values[0].longitude ) {
                for ( var i = 0; i < values.length; i++) {
                    values[i].date = formatDate(new Date(values[i].time * 1000));
                    values[i].value = values[i].latitude + "," + values[i].longitude;
                    delete values[i].latitude;
                    delete values[i].longitude;
                }
            }
        }

        ko.renderTemplate("details/datalist", {
            data : values,
            environment : environment
        }, {}, document.getElementById("dataList"));
        $('#graph').unblock();
        $("#graph").hide();
        $('#dataList').show();
        return;
    }
    else if( renderType=="map" ) {
        //load openlayers lib only when needed
        yepnope({
            load : 'js/libs/OpenLayers/OpenLayers.js',
            complete : function() {
                //configure openlayers lib
                OpenLayers.ImgPath = '/js/libs/OpenLayers/img/';

                //clear container
                $('#graph').empty();

                if( values.length>0 ) {
                    //create map, layers and projection
                    var map = new OpenLayers.Map('graph');
                    var layer = new OpenLayers.Layer.OSM();
                    var markers = new OpenLayers.Layer.Markers("Markers");
                    var vectors = new OpenLayers.Layer.Vector("Lines");
                    var fromProjection = new OpenLayers.Projection("EPSG:4326");
                    var toProjection = new OpenLayers.Projection("EPSG:900913");
                    var lineStyle = { strokeColor: '#FF0000',
                                      strokeOpacity: 0.5,
                                      strokeWidth: 5 };
        
                    //add layers
                    map.addLayer(layer);
                    map.addLayer(markers);
                    map.addLayer(vectors);

                    //add markers
                    var prevPoint = null;
                    var features = [];
                    for( var i=0; i<values.length; i++ ) {
                        var position = new OpenLayers.LonLat(values[i].longitude, values[i].latitude).transform(fromProjection, toProjection);
                        var point = new OpenLayers.Geometry.Point(values[i].longitude, values[i].latitude).transform(fromProjection, toProjection);
                        markers.addMarker(new OpenLayers.Marker(position));
                        if( prevPoint ) {
                            //join markers
                            var line = new OpenLayers.Geometry.LineString([prevPoint, point]);
                            features.push( new OpenLayers.Feature.Vector(line, null, lineStyle) );
                        }
                        prevPoint = point;
                    }
                    vectors.addFeatures(features);

                    //center map to first position
                    var zoom = 13;
                    map.setCenter( new OpenLayers.LonLat(values[0].longitude, values[0].latitude).transform(fromProjection, toProjection), zoom);
                }
                else {
                    notif.error('No data to display');
                }
                //show container
        	    $('#graph').unblock();
                $('#graph').show();
            }
        });
        return;
    }

	/* Split the values into buckets */
	var num_buckets = Math.max(1, Math.floor(values.length / max_ticks));
	var buckets = values.chunk(num_buckets);
	var labels = [];
	var i = 0;

	/*
	 * Compute averange for each bucket and pick a representative time to
	 * display
	 */
	for ( var j = 0; j < buckets.length; j++) {
	    var bucket = buckets[j];
	    var ts = bucket[0].time + (bucket[bucket.length - 1].time - bucket[0].time) / 2;
	    labels.push(new Date(Math.floor(ts) * 1000));
	    var value = 0;
	    for ( var k = 0; k < bucket.length; k++) {
		value += bucket[k].level;
	    }
	    data.push([ i, value / k ]);
	    i++;
	}

	/* Render the graph */
	var container = document.getElementById('graph');
	container._environment = environment;
	Flotr.draw(container, [ data ], {
	    HtmlText : false,
	    title : environment,
	    mode : "time",
	    yaxis : {
		tickFormatter : function(x) {
		    return x + " " + unit;
		},
	    },
	    mouse : {
		track : true,
		relative : true,

		trackFormatter : function(o) {
		    return formatDate(labels[Math.round(o.x)]) + " - " + o.y + " " + unit;
		}
	    },
	    xaxis : {
		noTicks : i,
		labelsAngle : 90,
		tickDecimals : 0,
		tickFormatter : function(x) {
		    return formatDate(labels[x]);
		}
	    }
	});

	/* We have no data ... */
	if (data.length == 0) {
	    var canvas = document.getElementsByClassName("flotr-overlay")[0];
	    var context = canvas.getContext("2d");
	    var x = canvas.width / 2;
	    var y = canvas.height / 2;

	    context.font = "30pt Arial";
	    context.textAlign = "center";
	    context.fillStyle = "red";
	    context.fillText('No data found for given time frame!', x, y);
	}

	$('#graph').unblock();
    }, 30);
}
