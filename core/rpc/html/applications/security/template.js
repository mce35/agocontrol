/**
 * Model class
 * 
 * @returns {SecurityConfig}
 */
function SecurityConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.securityController = null;
    self.alertControllers = [];
    self.currentPin = ko.observable('');
    self.housemodes = ko.observableArray([]);
    self.currentHousemode = ko.observable('');
    self.newHousemode = ko.observable('');
    self.zones = ko.observableArray([]);
    self.newZone = ko.observable('');
    self.possibleDelays = ko.observableArray([]);
    self.zoneMap = ko.observable([]);
    self.defaultHousemode = ko.observable();
    self.armedMessage = ko.observable('');
    self.disarmedMessage = ko.observable('');
    self.pinCallback = null;
    self.initialized = false;
    self.pinChecking = false;
    self.newPin = '';
    self.configChanged = ko.observable(false);
    
    //triggerable devices
    self.triggerableDevices = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.name() && device.name().length>0 )
            {
                //only keep binary sensors
                if( device.devicetype=='binarysensor' )
                {
                    output.push({uuid:device.uuid, name:device.name(), devicetype:device.devicetype});
                }
            }
        }
        return output;
    });

    //alarmable devices
    self.alarmableDevices = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.devicetype=='smsgateway' || device.devicetype=='smtpgateway' || device.devicetype=='twittergateway' || device.devicetype=='pushgateway' )
            {
                //name specified by user
                output.push({uuid:device.uuid, name:(device.name()||device.devicetype)+' (gateway)', devicetype:device.devicetype});
            }
            else if( device.name() && device.name().length>0 )
            {
                //only keep switch and scenarii
                if( device.devicetype=='switch' || device.devicetype=='scenario' )
                {
                    output.push({uuid:device.uuid, name:device.name()+' ('+device.devicetype+')', devicetype:device.devicetype});
                }
            }
        }

        return output;
    });

    //==================
    //GENERAL
    //==================
    
    //config changed
    self.uiConfigChanged = function()
    {
        self.configChanged(true);
    };
   
    //init security application
    self.init = function()
    {
        //get alert controllers
        var alertControllers = self.agocontrol.inventory.schema.categories.usernotification.devicetypes;

        //get useful controller uuids
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=="securitycontroller" )
            {
                //save security controller
                self.securityController = self.agocontrol.devices()[i].uuid;
                self.loadSecurityConfig();
            }
            else
            {
                for( var j=0; j<alertControllers.length; j++ )
                {
                    if( alertControllers[j]===self.agocontrol.devices()[i].devicetype )
                    {
                        self.alertControllers.push(self.agocontrol.devices()[i].uuid);
                    }
                }
            }
        }

        if( !self.securityController )
        {
            //no controller found
            notif.fatal('No security controller found!');
        }

        //generate possible delays
        self.possibleDelays.push({'value':-1, 'label':'inactive'});
        self.possibleDelays.push({'value':0, 'label':'immediate'});
        self.possibleDelays.push({'value':5, 'label':'5 seconds'});
        self.possibleDelays.push({'value':10, 'label':'10 seconds'});
        self.possibleDelays.push({'value':15, 'label':'15 seconds'});
        self.possibleDelays.push({'value':30, 'label':'30 seconds'});
        self.possibleDelays.push({'value':60, 'label':'1 minute'});
        self.possibleDelays.push({'value':120, 'label':'2 minutes'});
        self.possibleDelays.push({'value':300, 'label':'5 minutes'});
        self.possibleDelays.push({'value':600, 'label':'10 minutes'});

        //add event handler
        self.agocontrol.addEventHandler(self.eventHandler);
    };

    //agocontrol event handler
    self.eventHandler = function(event)
    {
        //catch new housemode
        if( event['event']=='event.security.housemodechanged' && event['housemode'] )
        {
            self.currentHousemode(event['housemode']);
        }
    };
    
    //update zonemap (handle all security data)
    self.zoneMap.subscribe(function()
    {
        var zoneMap = self.zoneMap();
        var housemodes = [];
        var zones = [];
        var zoneIdx = {};

        // Build list of zones and index mapping
        for ( var housemode in zoneMap)
        {
            for ( var i = 0; i < zoneMap[housemode].length; i++)
            {
                if (zoneIdx[zoneMap[housemode][i].zone] === undefined)
                {
                    zones.push({
                        name : zoneMap[housemode][i].zone
                    });
                    zoneIdx[zoneMap[housemode][i].zone] = i;
                }
            }
        }

        //Build a housemode list with delays, "inactive" means not set
        for ( var housemode in zoneMap)
        {
            var configs = [];

            //init default structure
            for( var i=0; i<zones.length; i++ )
            {
                var config = {
                    delay: ko.observable('inactive'),
                    devices: ko.observableArray([]),
                    alarms: ko.observableArray([]),
                    hm: null,
                    zn: null
                };
                configs.push(config);
            }

            //and fill structure with current config
            for( var i=0; i<zoneMap[housemode].length; i++ )
            {
                configs[zoneIdx[zoneMap[housemode][i].zone]].delay(zoneMap[housemode][i].delay);
                for( var j=0; j<zoneMap[housemode][i].alarms.length; j++ )
                {
                    var uuid = zoneMap[housemode][i].alarms[j];
                    var device = self.getDevice(uuid);
                    if( device )
                    {
                        //configs[zoneIdx[zoneMap[housemode][i].zone]].alarms.push(device);
                        configs[zoneIdx[zoneMap[housemode][i].zone]].alarms.push(uuid);
                    }
                    else
                    {
                        console.warn('Device "'+uuid+'" (alarm) not found. Maybe it has been removed.');
                    }
                }
                for( var j=0; j<zoneMap[housemode][i].devices.length; j++ )
                {
                    var uuid = zoneMap[housemode][i].devices[j];
                    var device = self.getDevice(uuid);
                    if( device )
                    {
                        //configs[zoneIdx[zoneMap[housemode][i].zone]].devices.push(device);
                        configs[zoneIdx[zoneMap[housemode][i].zone]].devices.push(uuid);
                    }
                    else
                    {
                        console.warn('Device "'+uuid+'" (device) not found. Maybe it has been removed.');
                    }
                }
                configs[zoneIdx[zoneMap[housemode][i].zone]].hm = housemode;
                configs[zoneIdx[zoneMap[housemode][i].zone]].zn = zoneMap[housemode][i].zone;
            }

            //append housemode
            housemodes.push({
                name : housemode,
                //deprecated delays : delays,
                configs : configs
            });
        }

        self.zones(zones);
        self.housemodes(housemodes);
    });

    //get device according to specified uuid
    self.getDevice = function(uuid)
    {
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.uuid==uuid )
            {
                return device;
            }
        }
        return null;
    };

    //==================
    //CONFIGURATION
    //==================
    
    //get current security config
    this.loadSecurityConfig = function()
    {
        //block ui
        self.agocontrol.block($('#securityTable'));

        var content = {};
        content.command = "getconfig";
        content.uuid = self.securityController;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( res && res.data.config )
                {
                    self.zoneMap(res.data.config);
                    self.currentHousemode(res.data.housemode);
                    self.armedMessage(res.data.armedMessage);
                    self.disarmedMessage(res.data.disarmedMessage);
                    self.defaultHousemode(res.data.defaultHousemode);
                    self.initialized = true;
                }
            })
            .finally(function() {
                //unblock ui
                self.agocontrol.unblock($('#securityTable'));
            });
    };

    //restore config from agosecurity
    self.restoreConfig = function()
    {
        //load config
        self.loadSecurityConfig();

        //update config flag
        self.configChanged(false);
    };

    //ui event to save config
    self.uiSaveConfig = function()
    {
        self.checkPin(self.saveConfig);
    }

    //save security configuration
    // /!\ need pin so open pin modal first and call this function in callback
    self.saveConfig = function()
    {
        //block ui
        self.agocontrol.block($('#securityTable'));

        var securityConf = {};
        //add housemodes and zones
        for( var i=0; i<self.housemodes().length; i++ )
        {
            var configs = [];
            for( var j=0; j<self.housemodes()[i].configs.length; j++ )
            {
                var config = {
                    'zone' : self.housemodes()[i].configs[j].zn,
                    'delay' : self.housemodes()[i].configs[j].delay(),
                    'devices' : self.housemodes()[i].configs[j].devices(),
                    'alarms' : self.housemodes()[i].configs[j].alarms()
                };
                configs.push(config);
            }
            securityConf[self.housemodes()[i].name] = configs;
        }

        var content = {};
        content.command = "setconfig";
        content.uuid = self.securityController;
        content.pin = self.currentPin();
        content.config = securityConf;
        content.armedMessage = self.armedMessage();
        content.disarmedMessage = self.disarmedMessage();
        content.defaultHousemode = self.defaultHousemode();
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                notif.success('Security config saved successfully');

                //update config flag
                self.configChanged(false);
            })
            .catch(function(err) {
                //error occured, restore config
                self.restoreConfig();
            })
            .finally(function() {
                self.agocontrol.unblock($('#securityTable'));
            });
    };
    
    //set current housemode
    // /!\ need pin
    self.setHousemode = function()
    {
        var content = {};
        content.uuid = self.securityController;
        content.command = 'sethousemode';
        content.pin = self.currentPin();
        content.housemode = self.currentHousemode();
        self.agocontrol.sendCommand(content)
            .catch(function(err) {
                //restore old housemode
                if( err.data && err.data.housemode )
                {
                    self.currentHousemode(err.data.housemode);
                }
            });
    };

    //==================
    //PIN TAB
    //==================

    //pin code key pressed
    self.keyPressed = function(key)
    {
        if( self.currentPin().length<4 )
        {
            //update current pin
            self.currentPin(self.currentPin()+(''+key));

            //check pin
            if( self.currentPin().length==4 )
            {
                //pin fully entered
                if( self.pinChecking )
                {
                    //pin checking
                    //close popup and launch callback
                    if( self.pinCallback )
                    {
                        self.pinCallback();
                        self.pinCallback = null;
                    }
                    self.closePinPanel();

                    //set flag
                    self.pinChecking = false;
                }
                else
                {
                    //save new pin code
                    self.newPin = self.currentPin();

                    //then update pin code after pin checking
                    self.checkPin(self.setPin);
                }
            }
        }
    };

    //reset entered code
    self.resetPin = function()
    {
        self.currentPin('');
    };

    //open pin panel
    self.openPinPanel = function()
    {
        //hack to avoid modal opens when html select element is populated
        if( self.initialized )
        {
            //reset pin code
            self.resetPin();

            //display modal
            $('#pinPanel').modal('show');
        }
    };

    //close pin panel
    self.closePinPanel = function()
    {
        //just hide modal
        $('#pinPanel').modal('hide');

        //and reset password
        self.resetPin();
    };

    //check pin code and execute callback
    self.checkPin = function(callback)
    {
        //set flag
        self.pinChecking = true;

        //set callback
        self.pinCallback = callback;
        
        //open pin panel
        self.openPinPanel();

        //finally callback is called when 4 digits pin code is entered
    };

    //set pin
    self.setPin = function()
    {
        var content = {};
        content.uuid = self.securityController;
        content.command = 'setpin';
        content.pin = self.currentPin();
        content.newpin = self.newPin;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                notif.success('Pin code changed successfully');
            })
            .finally(function() {
                //reset pin code
                self.resetPin();
            });
    }

    //==================
    //HOUSEMODES
    //==================
    
    //open housemode modal
    self.openHousemodeModal = function()
    {
        $('#housemodeModal').modal('show');
    };

    //define housemodes grid
    self.housemodesGrid = new ko.agoGrid.viewModel({
        data: self.housemodes,
        columns: [
            {headerText:'Housemode', rowText:'name'},
            {headerText:'Actions', rowText:''}
        ],
        //rowCallback: self.makeEditable,
        rowTemplate: 'housemodeTemplate',
        displaySearch: false,
        displayPagination: false
    });

    //Add new housemode
    self.addHousemode = function()
    {
        //check housemode validity
        if( $.trim(self.newHousemode()).length===0 )
        {
            notif.warning('Please specify valid housemode name');
            return;
        }

        //check if housemode already exists
        var exists = false;
        for( var i=0; i<self.housemodes().length; i++ )
        {
            if( self.housemodes()[i].name==self.newHousemode() )
            {
                exists = true;
                break;
            }
        }
        
        //add new housemode
        if( !exists )
        {
            var configs = [];
            for( var i=0; i<self.zones().length; i++ )
            {
                var config = {};
                config.delay = ko.observable('inactive');
                config.devices = ko.observableArray([]);
                config.alarms = ko.observableArray([]);
                config.hm = self.newHousemode();
                config.zn = self.zones()[i].name;
                configs.push(config);
            }
            var housemodes = self.housemodes();
            housemodes.push({
                'name': self.newHousemode(),
                'delays': [],
                'configs': configs
            });
            self.housemodes([]);
            self.housemodes(housemodes);
        }
        else
        {
            notif.error('Specified housemode already exists.');
        }

        //reset housemode
        self.newHousemode('');

        //set changes done
        self.configChanged(true);

        //close modal
        $('#housemodeModal').modal('hide');
    };

    //delete house mode
    self.delHousemode = function(housemode)
    {
        for( var i=0; i<self.housemodes().length; i++ )
        {
            if( self.housemodes()[i].name==housemode )
            {
                self.housemodes.splice(i, 1);
                break;
            }
        }

        //set changes done
        self.configChanged(true);
    };

    //==================
    //ZONES
    //==================
    
    //open zone modal
    self.openZoneModal = function()
    {
        $('#zoneModal').modal('show');
    };

    //define zones grid
    self.zonesGrid = new ko.agoGrid.viewModel({
        data: self.zones,
        columns: [
            {headerText:'Zone', rowText:'name'},
            {headerText:'Linked devices', rowText:'name'},
            {headerText:'Actions', rowText:''}
        ],
        //rowCallback: self.makeEditable,
        rowTemplate: 'zoneTemplate',
        displaySearch: false,
        displayPagination: false
    });

    //add new zone
    self.addZone = function()
    {
        //check if housemode exists
        if( self.housemodes().length===0 )
        {
            notif.info('Please create housemodes first');
            return;
        }

        //check zone name validity
        if( $.trim(self.newZone()).length===0 )
        {
            notif.warning('Please specify valid zone name');
            return;
        }
        
        //check if zone already exists
        var exists = false;
        for( var i=0; i<self.zones().length; i++ )
        {
            if( self.zones()[i].name==self.newZone() )
            {
                exists = true;
                break;
            }
        }
        
        //add new zone
        if( !exists )
        {
            self.zones.push({
                'name' : self.newZone()
            });

            //and add infos to housemodes
            var housemodes = self.housemodes();
            for( var i=0; i<housemodes.length; i++ )
            {
                var config = {};
                config.delay = ko.observable('inactive');
                config.devices = ko.observableArray([]);
                config.alarms = ko.observableArray([]);
                config.hm = housemodes[i].name;
                config.zn = self.newZone();
                housemodes[i].configs.push(config);
            }
            self.housemodes([]);
            self.housemodes(housemodes);
        }
        else
        {
            notif.error('Specified zone already exists.');
        }

        //reset zone
        self.newZone('');

        //set changes done
        self.configChanged(true);

        //close modal
        $('#zoneModal').modal('hide');
    };

    //delete zone
    self.delZone = function(zone)
    {
        var zoneIndex = -1;

        //delete specified zone
        for( var i=0; i<self.zones().length; i++ )
        {
            if( self.zones()[i].name==zone )
            {
                self.zones.splice(i, 1);
                zoneIndex = i;
                break;
            }
        }

        //update housemodes
        if( zoneIndex>=0 )
        {
            var housemodes = self.housemodes();
            for( var i=0; i<housemodes.length; i++ )
            {
                housemodes[i].configs.splice(zoneIndex, 1);
            }
            self.housemodes([]);
            self.housemodes(housemodes);
        }

        //set changes done
        self.configChanged(true);
    };
    
    //init application
    self.init();
    
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new SecurityConfig(agocontrol);

    model.listDeviceTemplate = function(item) {
        if( agocontrol.supported_devices().indexOf(item.devicetype)!=-1 )
        {
            return 'templates/list/' + item.devicetype;
        }
        return 'templates/list/empty';
    }.bind(model);

    model.housemodeDevicesTemplate = function(item) {
        return 'templates/list/' + item.devicetype;
    }.bind(model);

    return model;
}

/**
 * Reset template
 */
function reset_template(model)
{
    if( model )
    {
        model.agocontrol.removeEventHandler(model.eventHandler);
    }
}

