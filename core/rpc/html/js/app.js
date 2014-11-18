Array.prototype.chunk = function(chunkSize) {
    var array = this;
    return [].concat.apply([], array.map(function(elem, i) {
        return i % chunkSize ? [] : [ array.slice(i, i + chunkSize) ];
    }));
};

/**
 * Formats a date object
 * 
 * @param date
 * @param simple
 * @returns {String}
 */
function formatDate(date)
{
    return date.toLocaleDateString() + " " + date.toLocaleTimeString();
};

/**
 * Format specified string to lower case except first letter to uppercase
 */
function ucFirst(str)
{
    return str.charAt(0).toUpperCase() + str.slice(1).toLowerCase();
}


//View object
//dynamic page loading inspired from : http://jsfiddle.net/rniemeyer/PctJz/
function View(templateName, model)
{
    this.templateName = templateName;
    this.model = model; 
};

//Template object
function Template(path, resources, template, params)
{
    this.path = path;
    this.resources = resources;
    this.template = template;
    this.params = params; //data passed to loaded template
}

//Menu item object
function Item(key, data)
{
    this.key = key;
    this.data = data;
};


//Agocontrol object
function Agocontrol()
{
    //members
    var self = this;
    self.subscription = null;
    self.url = 'jsonrpc';
    self.devices = ko.observableArray([]);
    self.environment = ko.observableArray([]);
    self.rooms = ko.observableArray([]);
    self.schema = ko.observable();
    self.system = ko.observable(); 
    self.variables = ko.observableArray([]); //{}
    self.supported_devices = ko.observableArray([]);

    self.plugins = ko.observableArray([]);
    self.dashboards = ko.observableArray([]);
    self.configurations = ko.observableArray([]);

    self.agoController = null;
    self.scenarioController = null;
    self.eventController = null;

    //fill configurations
    //TODO return configurations by ago
    self.configurations.push({name:'dashboards', ucName:'Dashboards', path:'configuration/dashboards', template:'html/dashboards'});
    self.configurations.push({name:'cloud', ucName:'Cloud', path:'configuration/cloud', template:'html/cloud'});
    self.configurations.push({name:'devices', ucName:'Devices', path:'configuration/devices', template:'html/devices'});
    self.configurations.push({name:'events', ucName:'Events', path:'configuration/events', template:'html/events'});
    self.configurations.push({name:'rooms', ucName:'Rooms', path:'configuration/rooms', template:'html/rooms'});
    self.configurations.push({name:'scenarios', ucName:'Scenarios', path:'configuration/scenarios', template:'html/scenarios'});
    self.configurations.push({name:'security', ucName:'Security', path:'configuration/security', template:'html/security'});
    self.configurations.push({name:'variables', ucName:'Variables', path:'configuration/variables', template:'html/variables'});

    //send command   
    self.sendCommand = function(content, callback, timeout, async)
    {
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
    };

    //refresh devices list
    self.getDevices = function(async)
    {
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
                    console.log('Add new device ' + uuid);
                    console.log(devs[uuid]);
                    self.devices.push(new device(self, devs[uuid], uuid));
                }
            }
        }, async);
    };

    //refresh dashboards list
    self.getDashboards = function(async)
    {
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
    };

    //handle inventory
    self.handleInventory = function(response)
    {
        //check errors
        if (response != null && response.result.match !== undefined && response.result.match(/^exception/))
        {
            notif.error("RPC ERROR: " + response.result);
            return;
        }

        if (response == null)
        {
            response = {
                result : JSON.parse(localStorage.inventoryCache)
            };
        }

        //fill members
        self.dashboards.push({name:'all', ucName:'All my devices', action:'', editable:false});
        for( uuid in response.result.floorplans )
        {
            var dashboard = response.result.floorplans[uuid];
            dashboard.uuid = uuid;
            dashboard.action = '';
            dashboard.ucName = ucFirst(dashboard.name);
            dashboard.editable = true;
            self.dashboards.push(dashboard);
        }
        for( uuid in response.result.rooms )
        {
            var room = response.result.rooms[uuid];
            room.uuid = uuid;
            room.action = ''; //dummy for datatables
            self.rooms.push(room);
        }
        for( name in response.result.variables )
        {
            var variable = {
                variable: name,
                value: response.result.variables[name],
                action: '' //dummy for datatables
            };
            self.variables.push(variable);
        }
        self.system(response.result.system);
        self.schema(response.result.schema);

        //console.log('DASHBOARDS:');
        //console.log(self.dashboards());
        //console.log('RESPONSE:');
        //console.log(response);
        //floorPlans = response.result.floorplans;
        //environment = response.result.environment;
        
        //save device map
        //console.log("DEVICES");
        //console.log(response.result.devices);
        var devs = self.cleanInventory(response.result.devices);
        //console.log("DEVS");
        //console.log(devs);
        for( var uuid in devs )
        {
            if (devs[uuid].room !== undefined && devs[uuid].room)
            {
                devs[uuid].roomUID = devs[uuid].room;
                if( self.rooms()[devs[uuid].room]!==undefined )
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
            
            //TODO still useful?
            found = false;
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
            if (!found)
            {
                self.devices.push(new device(self, devs[uuid], uuid));
            }
        }
        
        //get plugins
        $.ajax({
            url : "cgi-bin/pluginlist.cgi",
            method : "GET",
            async : false,
        }).done(function(result) {
            //load plugins list template at top of menu
            for( var i=0; i<result.length; i++ )
            {
                if( result[i].name=='Applications list' )
                {
                    var plugin = result[i];
                    plugin.ucName = ucFirst(plugin.name);
                    self.plugins.push(plugin);
                    break;
                }
            }
            
            //load all other plugins
            for( var i=0; i<result.length; i++ )
            {
                if( result[i].name!='Applications list' )
                {
                    var plugin = result[i];
                    plugin.ucName = ucFirst(plugin.name);
                    self.plugins.push(plugin);
                }
            }
        });

        //get supported devices
        $.ajax({
            url : "cgi-bin/listing.cgi?devices=1",
            method : "GET",
            async : false
        }).done(function(msg) {
            self.supported_devices(msg);
        });
    };

    //get inventory
    self.getInventory = function(callback, async)
    {
        if( async===undefined )
        {
            async = false;
        }
        console.log('ASYNC='+async);
        if( !callback )
        {
            callback = self.handleInventory;
        }
        var content = {};
        content.command = "inventory";
        self.sendCommand(content, callback, 10, async);
    };

    //get event
    self.getEvent = function()
    {
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
    }

    //handle event
    self.handleEvent = function(requestSucceed, response)
    {
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
    };

    //clean inventory
    self.cleanInventory = function(data)
    {
        for ( var k in data)
        {
            if (!data[k])
            {
               delete data[k];
            }
        }
        return data;
    };

    self.subscribe = function()
    {
        var request = {};
        request.method = "subscribe";
        request.id = 1;
        request.jsonrpc = "2.0";
        $.post(self.url, JSON.stringify(request), self.handleSubscribe, "json");
    };

    self.unsubscribe = function()
    {
        //TODO fix issue: unsubscribe must be sync

        var request = {};
        request.method = "unsubscribe";
        request.id = 1;
        request.jsonrpc = "2.0";
        request.params = {};
        request.params.uuid = self.subscription;

        $.post(self.url, JSON.stringify(request), function() {}, "json");
    };

    self.handleSubscribe = function(response)
    {
        if (response.result)
        {
            self.subscription = response.result;
            self.getEvent();
            window.onbeforeunload = function(event)
            {
                self.unsubscribe();
            };
        }
    };

    //****************************************************************
    // UI helpers
    //****************************************************************

    //block specified element
    self.block = function(el)
    {
        $(el).block({ message: '<img src="img/loading.gif" />' });
    };

    //unblock specified element
    self.unblock = function(el)
    {
        $(el).unblock();
    };

    //avoid datatables's previous/next click to change current page
    //must be called after model is rendered, so call it in your model afterRender function
    self.stopDatatablesLinksPropagation = function()
    {
        var next = $('#configTable_next');
        var prev = $('#configTable_previous');
        if( next[0] && prev[0] )
        {
            next.click(function() {
                return false;
            });
            prev.click(function() {
                return false;
            });
        }
    };
};

//Agocontrol viewmodel
//based on knockout webmail example
//http://learn.knockoutjs.com/WebmailExampleStandalone.html
function AgocontrolViewModel()
{
    //MEMBERS
    var self = this;
    self.currentView = ko.observable();
    self.agocontrol = new Agocontrol();

    //disable click event
    self.noclick = function()
    {
        return false;
    };

    //route functions
    self.gotoDashboard = function(dashboard, edit)
    {
        console.log('GOTO DASHBOARD');
        console.log(dashboard);
        if( edit===true )
        {
            location.hash = 'dashboard/' + dashboard.name + '/edit';
        }
        else
        {
            location.hash = 'dashboard/' + dashboard.name;
        }
    };

    self.gotoConfiguration = function(config)
    {
        console.log('GOTO CONFIG');
        console.log(config);
        location.hash = 'config/' + config.name;
    };

    self.gotoPlugin = function(plugin)
    {
        console.log('GOTO PLUGIN');
        console.log(plugin);
        location.hash = 'plugin/' + plugin.name;
    };

    //load template
    self.loadTemplate = function(template)
    {
        //load js resource file
        console.log('LOAD TEMPLATE');
        console.log(template);

        //block ui
        $.blockUI({ message: '<img src="img/loading.gif" />  Loading...' });

        //load template script file
        $.getScript(template.path+'/template.js' , function()
        {
            //Load the plugins resources if any
            if( template.resources && ((template.resources.css && template.resources.css.length>0) || (template.resources.js && template.resources.js.length>0)) )
            {
                var resources = [];
                if (template.resources.css && template.resources.css.length > 0)
                {
                    for ( var i = 0; i < template.resources.css.length; i++)
                    {
                        resources.push(template.path + "/" + template.resources.css[i]);
                    }
                }
                if (template.resources.js && template.resources.js.length > 0)
                {
                    for ( var i = 0; i < template.resources.js.length; i++)
                    {
                        resources.push(template.path + "/" + template.resources.js[i]);
                    }
                }
                if (resources.length > 0)
                {
                    yepnope({
                        load : resources,
                        complete : function() {
                            // here, all resources are really loaded
                            if (typeof init_template == 'function')
                            {
                                var model = init_template(template.path+'/', template.params, self.agocontrol);
                                if( model )
                                {
                                    self.currentView(new View(template.path+'/'+template.template, model));
                                }
                                else
                                {
                                    console.log('Failed to load template: no model returned by init_template. Please check your code.');
                                    notif.fatal('Problem during page loading.');
                                }
                            }
                            else
                            {
                                console.log('Failed to load template: init_template function not defined. Please check your code.');
                                notif.fatal('Problem during page loading.');
                            }
                            $.unblockUI();
                        }
                    });
                }
            }
            else
            {
                if (typeof init_template == 'function')
                {
                    var model = init_template(template.path+'/', template.params, self.agocontrol);
                    if( model )
                    {
                        self.currentView(new View(template.path+'/'+template.template, model));
                    }
                    else
                    {
                        console.log('Failed to load template: no model returned by init_template. Please check your code.');
                        notif.fatal('Problem during page loading.');
                    }
                }
                else
                {
                    console.log('Failed to load template: init_template function not defined. Please check your code.');
                    notif.fatal('Problem during page loading.');
                }
                $.unblockUI();
            }
        }).fail(function(jqXHR, textStatus, errorThrown) {
            $.unblockUI();
            console.log("Failed to load template: ["+textStatus+"] "+errorThrown.message);
            console.log(errorThrown);
            notif.fatal('Problem during page loading.');
        });
    };

    //configure routes using sammy.js framework
    Sammy(function ()
    {
        //load agocontrol inventory
        self.agocontrol.getInventory(null, false);

        function getDashboard(name)
        {
            var dashboard = null;
            for( var i=0; i<self.agocontrol.dashboards().length; i++ )
            {
                if( self.agocontrol.dashboards()[i].name==name )
                {
                    dashboard = self.agocontrol.dashboards()[i];
                    break;
                }
            }
            return dashboard;
        };

        //dashboard loading
        this.get('#dashboard/:name', function()
        {
            if( this.params.name==='all' )
            {
                //special case for main dashboard (all devices)
                var basePath = 'dashboard/all';
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', null));
            }
            else
            {
                var dashboard = getDashboard(this.params.name);
                console.log(dashboard);
                if( dashboard )
                {
                    var basePath = 'dashboard/custom';
                    self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:false}));
                }
                else
                {
                    notif.fatal('Specified custom dashboard not found!');
                }
            }
        });

        //custom dashboard edition
        this.get('#dashboard/:name/edit', function()
        {
            var dashboard = getDashboard(this.params.name);
            if( dashboard )
            {
                var basePath = 'dashboard/custom';
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:true}));
            }
            else
            {
                notif.fatal('Specified custom dashboard not found!');
            }
        });

        //plugin loading
        this.get('#plugin/:name', function()
        {
            //get plugin infos from members
            var plugin = null;
            for( var i=0; i<self.agocontrol.plugins().length; i++ )
            {
                if( self.agocontrol.plugins()[i].name==this.params.name )
                {
                    //plugin found
                    plugin = self.agocontrol.plugins()[i];
                    break;
                }
            }
            if( plugin )
            {
                var basePath = "plugins/" + plugin._name;
                self.loadTemplate(new Template(basePath, plugin.resources, plugin.template, null));
            }
            else
            {
                notif.fatal('Specified plugin not found!');
            }
        });

        //configuration presentation loading
        this.get('#config', function()
        {
            var basePath = 'configuration/presentation';
            self.loadTemplate(new Template(basePath, null, 'html/presentation', null));
        });

        //configuration loading
        this.get('#config/:name', function()
        {
            console.log('-> configuration');
            //get config infos
            var config = null;
            for( var i=0; i<self.agocontrol.configurations().length; i++ )
            {
                if( self.agocontrol.configurations()[i].name==this.params.name )
                {
                    //config found
                    config = self.agocontrol.configurations()[i];
                }
            }
            console.log(config);
            if( config )
            {
                self.loadTemplate(new Template(config.path, null, config.template, null));
            }
            else
            {
                notif.fatal('Specified config page not found');
            }
        });

        //help
        this.get('#help/:name', function()
        {
            console.log('-> help');
            var basePath = 'help/' + this.params.name;
            var templatePath = '/html/' + this.params.name;
            self.loadTemplate(new Template(basePath, null, templatePath, null));
        });

        //startup page (dashboard)
        this.get('', function ()
        {
            console.log('-> startup');
            this.app.runRoute('get', '#dashboard/all');
        });
    }).run();

    self.agocontrol.subscribe();
};

ko.applyBindings(new AgocontrolViewModel());

