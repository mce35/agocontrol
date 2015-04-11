//Agocontrol object
function Agocontrol()
{
    this._init();
};

Agocontrol.prototype = {
    //private members
    subscription: null,
    url: 'jsonrpc',
    multigraphThumbs: [],
    deferredMultigraphThumbs: [],
    refreshMultigraphThumbsInterval: null,
    eventHandlers: [],
    _allApplications: ko.observableArray([]),
    _getApplications: Promise.pending(),
    _favorites: ko.observable(),
    _noProcesses: ko.observable(false),

    //public members
    devices: ko.observableArray([]),
    environment: ko.observableArray([]),
    rooms: ko.observableArray([]),
    schema: ko.observable(),
    system: ko.observable(),
    variables: ko.observableArray([]),
    supported_devices: ko.observableArray([]),
    processes: ko.observableArray([]),
    applications: ko.observableArray([]),
    dashboards: ko.observableArray([]),
    configurations: ko.observableArray([]),
    helps: ko.observableArray([]),

    agoController: null,
    scenarioController: null,
    eventController: null,
    inventory: null,
    dataLoggerController: null,
    systemController: null,

    _init : function(){
        /**
         * Update application list when we have raw list of application, favorites
         * and processes list
         */
        ko.computed(function(){
            var allApplications = this._allApplications();
            var favorites = this._favorites();
            var processes = this.processes();
            // Hack to get around lack of process list on FreeBSD
            var noProcesses = this._noProcesses();

            if(!allApplications.length || !favorites || (!noProcesses && !processes.length))
            {
                //console.log("Not all data ready, trying later");
                return;
            }

            var applications = [];
            //always display "application list" app on top of list
            for( var i=0; i<allApplications.length; i++ )
            {
                var application = allApplications[i];
                if( application.name=='Application list' )
                {
                    application.favorite = true; //applications always displayed
                    application.fav = ko.observable(application.favorite);
                    applications.push(application);
                    break;
                }
            }

            for( var i=0; i<allApplications.length; i++ )
            {
                var application = allApplications[i];
                var append = false;

                if( application.name!='Application list' )
                {
                    application.favorite = !!favorites[application.dir];
                    if( application.depends===undefined ||
                            (application.depends!==undefined && $.trim(application.depends).length==0) )
                    {
                        append = true;
                    }
                    else
                    {
                        //check if process is installed (and not if it's currently running!)
                        var proc = this.findProcess(application.depends);
                        if( proc )
                        {
                            append = true;
                        }
                    }
                }

                if(append)
                {
                    application.fav = ko.observable(application.favorite);
                    applications.push(application);
                }
            }

            this.applications(applications);
            this._getApplications.fulfill();
        }, this);
    },

    /**
     * Main entrypoint for application.
     * Fetches inventory and other important data. The returned deferred
     * is resolved when basic stuff such as inventory, applications, help pages
     * etc have been loaded (getInventory + updateListing).
     *
     * Application availability is NOT guaranteed to be loaded immediately.
     */
    initialize : function() {
        var p1 = this.getInventory()
            .then(this.handleInventory.bind(this));
        var p2 = this.updateListing();

        // Required but non-dependent
        this.updateFavorites();

        return Promise.all([p1, p2]);
    },


    /**
     * Send a command to an arbitrary Ago component.
     *
     * (if oldstyleCallback is ommited and a numeric is passed as second parameter, 
     * we will use that as timeout)
     * 
     * @param content Should be a dict with any parameters to broadcast to the qpid bus.
     * @param oldstyleCallback A deprecated callback; do not use for new code!
     * @param timeout How many seconds we want the RPC gateway to wait for response
     *
     * @return A promise which will be either resolved or rejected
     */
    sendCommand: function(content, oldstyleCallback, timeout)
    {
        if(oldstyleCallback !== undefined && timeout === undefined &&
                $.isNumeric(oldstyleCallback))
        {
            // Support sendCommand(content, timeout)
            timeout = oldstyleCallback;
            oldstyleCallback = null;
        }

        var self = this;
        var request = {};
        request.jsonrpc = "2.0";
        request.id = 1;
        request.method = "message";
        request.params = {};
        request.params.content = content;

        if (timeout)
        {
            request.params.replytimeout = timeout;
        }

        var promise = new Promise(function(resolve, reject){
            $.ajax({
                    type : 'POST',
                    url : self.url,
                    data : JSON.stringify(request),
                    dataType : "json",
                    success: function(r, textStatus, jqXHR) {
                        // JSON-RPC call gave JSON-RPC response
         
                        // Old-style callback users
                        if (oldstyleCallback)
                        {
                            // deep copy; we do not want to modify the response sent to new style
                            // handlers
                            var old = {};
                            $.extend(true, old, r);
                            if(old._temp_newstyle_response)
                            {
                                // New-style backend, but old-style UI code.
                                // Add some magic to fool the not-yet-updated code which is
                                // checking for returncode

                                if(old.result && !old.result.returncode)
                                {
                                    old.result.returncode = "0";
                                }
                                else if(old.error)
                                {
                                    if(!old.result)
                                        old.result = {};

                                    old.result.returncode = "-1";
                                }

                                delete old._temp_newstyle_response;
                            }
                            else if(old.error && old.error.identifier === 'error.no.reply')
                            {
                                // Previously agorpc added a result no-reply rather
                                // than using an error..
                                old.result = 'no-reply';
                            }

                            oldstyleCallback(old);
                        }

                        // New-style code should use promise pattern instead,
                        // which is either resolved or rejected with result OR error
                        if(r.result)
                        {
                            resolve(r.result);
                        }
                        else
                        {
                            reject(r.error);
                        }
                    },
                    error: function(jqXHR, textStatus, errorThrown)
                    {
                        // Failed to talk properly with agorpc
                        console.error('sendCommand failed for:');
                        console.error(content);
                        console.error('with errors:');
                        console.error(arguments);

                        // old: oldstyleCallback was never called on errors.

                        // Simulate JSON-RPC error
                        reject({
                                code:-32603,
                                message: 'transport.error',
                                data:{
                                    description:'Failed to talk with agorpc'
                                }
                            });
                    }
                });// $.ajax()
            }); // Promise()

        /* Attach a general .catch which logs all messages to notif
         * Note that the application code can still add any .catch handlers
         * on the returned promise, if required.
         */
        promise.catch(function(error){
            if( error.message && error.message==='no.reply' )
            {
                //controller is not responding
                notif.fatal('Controller is not responding');
            }
            else if( error.data && error.data.description )
            {
                notif.error(error.data.description);
            }
            else if( error.message && error.message )
            {
                notif.error(error.message);
            }else
                notif.error("Failed: TODO improve: " + JSON.stringify(error));
        });

        return promise;
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
    refreshDevices: function()
    {
        var self = this;

        //TODO for now refresh all inventory
        self.getInventory()
            .then(function(result) {
                var devs = self.cleanInventory(result.data.devices);
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
            });
    },

    //refresh dashboards list
    refreshDashboards: function()
    {
        var self = this;

        //TODO for now refresh all inventory
        self.getInventory()
            .then(function(result){
                self.handleDashboards(result.data.floorplans);
            });
    },

    //get inventory
    getInventory: function()
    {
        var self = this;
        var content = {};
        content.command = "inventory";
        return self.sendCommand(content);
    },

    //handle inventory
    handleInventory: function(result)
    {
        var self = this;

        //INVENTORY
        var inv = self.inventory = result.data;

        //rooms
        for( uuid in inv.rooms )
        {
            var room = inv.rooms[uuid];
            room.uuid = uuid;
            room.action = ''; //dummy for datatables
            self.rooms.push(room);
        }

        //variables
        for( name in inv.variables )
        {
            var variable = {
                variable: name,
                value: inv.variables[name],
                action: '' //dummy for datatables
            };
            self.variables.push(variable);
        }

        //system
        self.system(inv.system);

        //schema
        self.schema(inv.schema);

        //devices
        var devs = self.cleanInventory(inv.devices);
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

        // Handle dashboards/floorplans
        self.handleDashboards(inv.floorplans);

        // Devices loaded, we now have systemController property set..via device
        self.updateProcessList();
    },

    // Handle dashboard-part of inventory
    handleDashboards : function(floorplans) {
        var dashboards = [];
        dashboards.push({name:'all', ucName:'All my devices', action:'', editable:false});
        for( uuid in floorplans )
        {
            var dashboard = floorplans[uuid];
            dashboard.uuid = uuid;
            dashboard.action = '';
            dashboard.ucName = dashboard.name;
            dashboard.editable = true;
            dashboards.push(dashboard);
        }
        this.dashboards.replaceAll(dashboards);
    },

    // Fetch process-list from agosystem
    updateProcessList : function() {
        var self = this;
        var content = {};
        content.command = "getprocesslist";
        content.uuid = self.systemController;
        self.sendCommand(content, 1)
            .then(function(res) {
                var values = [];
                for( var procName in res.data )
                {
                    var proc = res.data[procName];
                    proc.name = procName;
                    values.push(proc);
                }

                self.processes.pushAll(values);
            })
            .catch(function(err){
                notif.warning('Unable to get processes list, Applications will not be available: '
                        + getErrorMessage(err));
                self._noProcesses(true);
            });
    },

    updateFavorites: function() {
        var self = this;

        //FAVORITES
        $.ajax({
            url: "cgi-bin/ui.cgi?param=favorites",
            method: "GET"
        }).done(function(res) {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply' && res.result==1 )
            {
                self._favorites(res.content);
            }
            else
            {
                if( res.result.error )
                {
                    console.error('Unable to get favorites: '+res.result.error);
                }
                else
                {
                    console.error('Unable to get favorites');
                }
            }
        });
    },

    updateListing: function(){
        var self = this;
        return $.ajax({
            url : "cgi-bin/listing.cgi?get=all",
            method : "GET"
        }).done(function(result) {
            //APPLICATIONS
            var applications = [];
            for( var i=0; i<result.applications.length; i++ )
            {
                var application = result.applications[i];
                application.ucName = ucFirst(application.name);
                applications.push(application);
            }

            // Update internal observable
            self._allApplications(applications);

            //CONFIGURATION PAGES
            var categories = {};
            for( var i=0; i<result.config.length; i++ )
            {
                //check missing category
                if( result.config[i].category===undefined )
                {
                    result.config[i].category = 'Uncategorized';
                }
                //add new category if necessary
                var category = ucFirst(result.config[i].category);
                if( categories[category]===undefined )
                {
                    categories[category] = [];
                }
                //append page to its category
                categories[category].push(result.config[i]);
            }

            var configurations = [];
            for( var category in categories )
            {
                var subMenus = [];
                for( var i=0; i<categories[category].length; i++ )
                {
                    subMenus.push(categories[category][i]);
                }
                if( subMenus.length==1 )
                {
                    //no submenu
                    configurations.push({'menu':subMenus[0], 'subMenus':null});
                }
                else
                {
                    //submenus
                    configurations.push({'menu':category, 'subMenus':subMenus});
                }
            }
            self.configurations.replaceAll(configurations);

            //SUPPORTED DEVICES
            self.supported_devices(result.supported)

            //HELP PAGES
            var helps = [];
            for( var i=0; i<result.help.length; i++ )
            {
                var help = result.help[i];
                help.url = null;
                helps.push(help);
            }
            helps.push({name:'Wiki', url:'http://wiki.agocontrol.com/'});
            helps.push({name:'About', url:'http://www.agocontrol.com/about/'});
            self.helps.replaceAll(helps);
        });
    },

    getApplication: function(appName) {
        var self = this;
        return this._getApplications.promise
            .then(function(){
                var apps = self.applications();
                for(var i=0; i < apps.length; i++) {
                    if(apps[i].name == appName)
                    {
                        return apps[i];
                    }
                }
                return null;
            });
    },
    getDashboard:function(name){
        var dashboards = this.dashboards();
        for( var i=0; i < dashboards.length; i++ )
        {
            if(dashboards[i].name == name )
            {
                return dashboards[i];
            }
        }
        return null;
    },

    //get event
    getEvent: function()
    {
        var self = this;

        // Long-poll for events
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
            //send event to other handlers
            for( var i=0; i<self.eventHandlers.length; i++ )
            {
                self.eventHandlers[i](response.result);
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
                            self.getInventory()
                                .then(function(result) {
                                    var tmpInv = self.cleanInventory(result.data.devices);
                                    var uuid = result.data.uuid;
                                    if (tmpInv[uuid] !== undefined)
                                    {
                                        if (tmpInv[uuid].values)
                                        {
                                            self.devices()[i].values(tmpInv[uuid].values);
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

    //add event handler
    //useful to get copy of received event, like in agodrain
    addEventHandler: function(callback)
    {
        var self = this;
        if( callback )
        {
            self.eventHandlers.push(callback);
        }
    },

    //remove specified event handler
    removeEventHandler: function(callback)
    {
        var self = this;
        var index = self.eventHandlers.indexOf(callback);
        if( callback && index!==-1 )
        {
            self.eventHandlers.splice(index, 1);
        }
        else
        {
            console.error('Unable to remove callback from eventHandlers list because callback was not found!');
        }
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

