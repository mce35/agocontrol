//Agocontrol object
function Agocontrol()
{
};

Agocontrol.prototype = {
    //private members
    subscription: null,
    url: 'jsonrpc',
    multigraphThumbs: [],
    deferredMultigraphThumbs: [],

    //public members
    devices: ko.observableArray([]),
    environment: ko.observableArray([]),
    rooms: ko.observableArray([]),
    schema: ko.observable(),
    system: ko.observable(), 
    variables: ko.observableArray([]),
    supported_devices: ko.observableArray([]),
    processes: ko.observableArray([]),

    plugins: ko.observableArray([]),
    dashboards: ko.observableArray([]),
    configurations: ko.observableArray([]),
    helps: ko.observableArray([]),

    agoController: null,
    scenarioController: null,
    eventController: null,
    inventory: null,
    dataLoggerController: null,
    systemController: null,

    //send command   
    sendCommand: function(content, callback, timeout, async)
    {
        var self = this;

        if( async===undefined )
        {
            async = true;
        }
        else
        {
            async = false;
        }

        var request = {};
        request.method = "message";
        request.params = {};
        request.params.content = content;
        if (timeout)
        {
            request.params.replytimeout = timeout;
        }
        request.id = 1;
        request.jsonrpc = "2.0";

        $.ajax({
            type : 'POST',
            url : self.url,
            data : JSON.stringify(request),
            success : function(r) {
                if (callback !== undefined)
                {
                    callback(r);
                }
            },
            dataType : "json",
            async : async
        });
    },

    //find room
    findRoom: function(uuid)
    {
        var self = this;

        for( var i=0; i<self.rooms().length; i++ )
        {
            if( self.rooms()[i].uuid==uuid )
            {
                return self.rooms()[i];
            }
        }
        return null;
    },

    //return specified process or null if not found
    findProcess: function(proc)
    {
        var self = this;
        if( proc && $.trim(proc).length>0 && self.processes().length>0 )
        {
            for( var i=0; i<self.processes().length; i++ )
            {
                if( self.processes()[i].name===proc )
                {
                    //process found
                    return self.processes()[i];
                }
            }
        }
        return null;
    },

    //refresh devices list
    refreshDevices: function(async)
    {
        var self = this;

        //TODO for now refresh all inventory
        self.getInventory(function(response) {
            var devs = self.cleanInventory(response.result.devices);
            for( var uuid in devs )
            {
                if (devs[uuid].room !== undefined && devs[uuid].room)
                {
                    devs[uuid].roomUID = devs[uuid].room;
                    if (self.rooms()[devs[uuid].room] !== undefined)
                    {
                        devs[uuid].room = self.rooms()[devs[uuid].room].name;
                    }
                    else
                    {
                        devs[uuid].room = "";
                    }
                }
                else
                {
                    devs[uuid].room = "";
                }
            
                var found = false;
                for ( var i=0; i<self.devices().length; i++)
                {
                    if( self.devices()[i].uuid===uuid )
                    {
                        // device already exists in devices array. Update its content
                        self.devices()[i].update(devs[uuid], uuid);
                        found = true;
                        break;
                    }
                }
                if( !found )
                {
                    //add new device
                    self.devices.push(new device(self, devs[uuid], uuid));
                }
            }
        }, async);
    },

    //refresh dashboards list
    refreshDashboards: function(async)
    {
        var self = this;

        //TODO for now refresh all inventory
        self.getInventory(function(response) {
            self.dashboards.removeAll();
            self.dashboards.push({name:'all', ucName:'All my devices', action:'', editable:false});
            for( uuid in response.result.floorplans )
            {
                //add new items
                var dashboard = response.result.floorplans[uuid];
                dashboard.uuid = uuid;
                dashboard.action = '';
                dashboard.ucName = ucFirst(dashboard.name);
                dashboard.editable = true;
                self.dashboards.push(dashboard);
            }
        }, async);
    },

    //handle inventory
    handleInventory: function(response)
    {
        var self = this;

        //check errors
        if (response != null && response.result.match !== undefined && response.result.match(/^exception/))
        {
            notif.error("RPC ERROR: " + response.result);
            return;
        }

        //INVENTORY
        self.inventory = response.result;

        //rooms
        for( uuid in response.result.rooms )
        {
            var room = response.result.rooms[uuid];
            room.uuid = uuid;
            room.action = ''; //dummy for datatables
            self.rooms.push(room);
        }

        //variables
        for( name in response.result.variables )
        {
            var variable = {
                variable: name,
                value: response.result.variables[name],
                action: '' //dummy for datatables
            };
            self.variables.push(variable);
        }

        //system
        self.system(response.result.system);

        //schema
        self.schema(response.result.schema);

        //devices
        var devs = self.cleanInventory(response.result.devices);
        for( var uuid in devs )
        {
            //update device room infos
            if( devs[uuid].room && devs[uuid].room.length>0 )
            {
                var r = self.findRoom(devs[uuid].room);
                if( r )
                {
                    //save room uuid in roomUID field instead of room field
                    devs[uuid].roomUID = devs[uuid].room;
                    //and put room name in room field
                    devs[uuid].room = r.name;
                }
                else
                {
                    //room not found, maybe it has been deleted
                    devs[uuid].roomUID = null;
                    devs[uuid].room = '';
                }
            }
            else
            {
                //only add new field
                devs[uuid].roomUID = null;
            }
            self.devices.push(new device(self, devs[uuid], uuid));
        }

        //PROCESSES (from agosystem)
        var content = {};
        content.command = "status";
        content.uuid = self.systemController;
        self.sendCommand(content, function(res) {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                for( var procName in res.result )
                {
                    var proc = res.result[procName];
                    proc.name = procName;
                    self.processes.push(proc);
                }
            }
            else
            {
                console.error('Unable to get processes list!');
            }
        }, 5, false);
        
        //LISTING
        //dashboards
        self.dashboards.push({name:'all', ucName:'All my devices', action:'', editable:false});
        for( uuid in response.result.floorplans )
        {
            var dashboard = response.result.floorplans[uuid];
            dashboard.uuid = uuid;
            dashboard.action = '';
            dashboard.ucName = dashboard.name;
            dashboard.editable = true;
            self.dashboards.push(dashboard);
        }
        
        $.ajax({
            url : "cgi-bin/listing.cgi?get=all",
            method : "GET",
            async : false,
        }).done(function(result) {

            //PLUGINS
            //load plugins list at top of menu
            for( var i=0; i<result.plugins.length; i++ )
            {
                if( result.plugins[i].name=='Applications list' )
                {
                    var plugin = result.plugins[i];
                    plugin.ucName = ucFirst(plugin.name);
                    plugin.favorite = true; //applications always displayed
                    plugin.fav = ko.observable(plugin.favorite);
                    self.plugins.push(plugin);
                    break;
                }
            }
            //load all other plugins
            for( var i=0; i<result.plugins.length; i++ )
            {
                if( result.plugins[i].name!='Applications list' )
                {
                    var append = false;
                    if( result.plugins[i].depends===undefined || (result.plugins[i].depends!==undefined && $.trim(result.plugins[i].depends).length==0) )
                    {
                        append = true;
                    }
                    else
                    {
                        //check if process is installed (and not if it's currently running!)
                        var proc = self.findProcess(result.plugins[i].depends);
                        if( proc )
                        {
                            append = true;
                        }
                    }

                    if( append )
                    {
                        var plugin = result.plugins[i];
                        plugin.ucName = ucFirst(plugin.name);
                        if( plugin.favorite===undefined )
                        {
                            plugin.favorite = false;
                        }
                        plugin.fav = ko.observable(plugin.favorite);
                        self.plugins.push(plugin);
                    }
                }
            }

            //CONFIGURATION PAGES
            for( var i=0; i<result.config.length; i++ )
            {
                self.configurations.push(result.config[i]);
            }

            //SUPPORTED DEVICES
            self.supported_devices(result.supported)

            //HELP PAGES
            for( var i=0; i<result.help.length; i++ )
            {
                var help = result.help[i];
                help.url = null;
                self.helps.push(help);
            }
            self.helps.push({name:'Wiki', url:'http://wiki.agocontrol.com/'});
            self.helps.push({name:'About', url:'http://www.agocontrol.com/about/'});
        });
    },

    //get inventory
    getInventory: function(callback, async)
    {
        var self = this;

        if( async===undefined )
        {
            async = false;
        }
        if( !callback )
        {
            callback = self.handleInventory.bind(self);
        }
        var content = {};
        content.command = "inventory";
        self.sendCommand(content, callback, 10, async);
    },

    //get event
    getEvent: function()
    {
        var self = this;

        var request = {};
        request.method = "getevent";
        request.params = {};
        request.params.uuid = self.subscription;
        request.id = 1;
        request.jsonrpc = "2.0";

        //$.post(url, JSON.stringify(request), null, "json")
        $.ajax({
            type: 'POST',
            url: self.url,
            data: JSON.stringify(request),
            dataType: 'json',
            async: true,
            success: function(data, textStatus, jqXHR)
            {
                //request succeed
                if( data.error!==undefined )
                {
                    if(data.error.code == -32602)
                    {
                        // Subscription not found, server restart or we've been gone
                        // for too long. Setup new subscription
                        self.subscribe();
                        return;
                    }

                    // request timeout (server side), continue polling
                    self.handleEvent(false, data);
                }
                else
                {
                    self.handleEvent(true, data);
                }
            },
            error: function(jqXHR, textStatus, errorThrown)
            {
                //request failed, retry in a bit
                setTimeout(function()
                {
                    self.getEvent();
                }, 1000);
            }
        });
    },

    //handle event
    handleEvent: function(requestSucceed, response)
    {
        var self = this;

        if( requestSucceed )
        {
            if (response.result.event == "event.security.countdown" && !securityPromted)
            {
                securityPromted = true;
                var pin = window.prompt("Alarm please entry pin:");
                var content = {};
                content.command = "cancel";
                content.uuid = response.result.uuid;
                content.pin = pin;
                self.sendCommand(content, function(res) {
                    if (res.result.error) 
                    {
                        notif.error(res.result.error);
                    }
                    securityPromted = false;
                });
                return;
            }
            else if (response.result.event == "event.security.intruderalert")
            {
                notif.error("INTRUDER ALERT!");
                return;
            }

            for ( var i = 0; i < self.devices().length; i++)
            {
                if (self.devices()[i].uuid == response.result.uuid )
                {
                    // update device last seen datetime
                    self.devices()[i].timeStamp(formatDate(new Date()));
                    //update device level
                    if( response.result.level !== undefined)
                    {
                        // update custom device member
                        if (response.result.event.indexOf('event.device') != -1 && response.result.event.indexOf('changed') != -1)
                        {
                            // event that update device member
                            var member = response.result.event.replace('event.device.', '').replace('changed', '');
                            if (self.devices()[i][member] !== undefined)
                            {
                                self.devices()[i][member](response.result.level);
                            }
                        }
                        // Binary sensor has its own event
                        else if (response.result.event == "event.security.sensortriggered")
                        {
                            if (self.devices()[i]['state'] !== undefined)
                            {
                                self.devices()[i]['state'](response.result.level);
                            }
                        }
                    }

                    //update device stale
                    if( response.result.event=="event.device.stale" && response.result.stale!==undefined )
                    {
                        self.devices()[i]['stale'](response.result.stale);
                    }

                    //update quantity
                    if (response.result.quantity)
                    {
                        var values = self.devices()[i].values();
                        //We have no values so reload from inventory
                        if (values[response.result.quantity] === undefined)
                        {
                            self.getInventory(function(inv)
                            {
                                var tmpInv = self.cleanInventory(inv.result.devices);
                                if (tmpInv[response.result.uuid] !== undefined)
                                {
                                    if (tmpInv[response.result.uuid].values)
                                    {
                                        self.devices()[i].values(tmpInv[response.result.uuid].values);
                                    }
                                }
                            });
                            break;
                        }

                        if( response.result.level !== undefined )
                        {
                           if( response.result.quantity==='forecast' && typeof response.result.level=="string" )
                            {
                                //update forecast value for barometer sensor only if string specified
                                self.devices()[i].forecast(response.result.level);
                            }
                            //save new level
                            values[response.result.quantity].level = response.result.level;
                        }
                        else if( response.result.latitude!==undefined && response.result.longitude!==undefined )
                        {
                            values[response.result.quantity].latitude = response.result.latitude;
                            values[response.result.quantity].longitude = response.result.longitude;
                        }

                        self.devices()[i].values(values);
                    }

                    break;
                }
            }
        }
        self.getEvent();
    },

    //clean inventory
    cleanInventory: function(data)
    {
        var self = this;

        for ( var k in data)
        {
            if (!data[k])
            {
               delete data[k];
            }
        }
        return data;
    },

    subscribe: function()
    {
        var self = this;

        var request = {};
        request.method = "subscribe";
        request.id = 1;
        request.jsonrpc = "2.0";
        $.post(self.url, JSON.stringify(request), self.handleSubscribe.bind(this), "json");
    },

    unsubscribe: function()
    {
        var self = this;

        //TODO fix issue: unsubscribe must be sync

        var request = {};
        request.method = "unsubscribe";
        request.id = 1;
        request.jsonrpc = "2.0";
        request.params = {};
        request.params.uuid = self.subscription;

        $.post(self.url, JSON.stringify(request), function() {}, "json");
    },

    handleSubscribe: function(response)
    {
        var self = this;

        if (response.result)
        {
            self.subscription = response.result;
            self.getEvent();
            window.onbeforeunload = function(event)
            {
                self.unsubscribe();
            };
        }
    },

};

