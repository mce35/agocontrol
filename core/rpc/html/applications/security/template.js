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
    self.alertController = null;
    self.alertEmailConfigured = ko.observable(false);
    self.alertSmsConfigured = ko.observable(false);
    self.alertTwitterConfigured = ko.observable(false);
    self.alertPushConfigured = ko.observable(false);
    self.currentPin = ko.observable('');
    self.housemodes = ko.observableArray([]);
    self.currentHousemode = ko.observable('');
    self.newHousemode = ko.observable('');
    self.zones = ko.observableArray([]);
    self.newZone = ko.observable('');
    self.possibleDelays = ko.observableArray([]);
    self.zoneMap = ko.observable([]);
    self.editorOpened = false;
    self.defaultHousemode = ko.observable();
    self.armedMessage = ko.observable('');
    self.disarmedMessage = ko.observable('');
    self.pinCallback = null;
    self.initialized = false;
    self.pinChecking = false;
    self.newPin = '';
    
    //==================
    //EDITOR
    //==================
    
    //open editor
    self.openEditor = function()
    {
        $('#housemodeEditor').slideDown({
            duration: 300,
            complete: function() {
            },
            always: function() {
                self.editorOpened = true;

                //editor opened, init draggables and droppables
                self.enableDragAndDrop();
            }
        });
    };

    //close editor
    self.closeEditor = function()
    {
        $('#housemodeEditor').slideUp({
            duration: 300,
            complete: function() {
            },
            always: function() {
                self.editorOpened = false;

                //close editor, disable edition mode
                self.disableDragAndDrop();
            }
        });
    };
    
    //add droppable item to device list (ensure there is no duplicate)
    //@param type: <device|alarm>
    self.addDroppableItem = function(type, housemode, zone, uuid)
    {
        //get device
        device = self.getDevice(uuid);
        if( device!==null )
        {
            //add device to housemode device list
            for( var i=0; i<self.housemodes().length; i++ )
            {
                if( self.housemodes()[i].name==housemode )
                {
                    for( var j=0; j<self.housemodes()[i].configs.length; j++ )
                    {
                        if( self.housemodes()[i].configs[j].zn==zone )
                        {
                            //housemode found
                            //check if device wasn't already added
                            var added = false;
                            if( type=='device' )
                            {
                                for( var k=0; k<self.housemodes()[i].configs[j].devices().length; k++ )
                                {
                                    if( self.housemodes()[i].configs[j].devices()[k].uuid===uuid )
                                    {
                                        //device already added
                                        added = true;
                                        break;
                                    }
                                }
                                //add device
                                if( !added )
                                {
                                    self.housemodes()[i].configs[j].devices.push(device);
                                }
                            }
                            else if( type=='alarm' )
                            {
                                for( var k=0; k<self.housemodes()[i].configs[j].alarms().length; k++ )
                                {
                                    if( self.housemodes()[i].configs[j].alarms()[k].uuid===uuid )
                                    {
                                        //alarm already added
                                        added = true;
                                        break;
                                    }
                                }
                                //add device
                                if( !added )
                                {
                                    self.housemodes()[i].configs[j].alarms.push(device);
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }
        else
        {
            console.error('Device with uuid "'+uuid+'" was not found');
        }
    };

    //remove droppable item
    self.removeDroppableItem = function(type, housemode, zone, uuid)
    {
        //add device to housemode device list
        for( var i=0; i<self.housemodes().length; i++ )
        {
            if( self.housemodes()[i].name==housemode )
            {
                for( var j=0; j<self.housemodes()[i].configs.length; j++ )
                {
                    if( self.housemodes()[i].configs[j].zn==zone )
                    {
                        //housemode found
                        //check if device wasn't already added
                        if( type=='device' )
                        {
                            for( var k=0; k<self.housemodes()[i].configs[j].devices().length; k++ )
                            {
                                if( self.housemodes()[i].configs[j].devices()[k].uuid===uuid )
                                {
                                    //delete item
                                    self.housemodes()[i].configs[j].devices.splice(k,1);
                                    break;
                                }
                            }
                        }
                        else if( type=='alarm' )
                        {
                            for( var k=0; k<self.housemodes()[i].configs[j].alarms().length; k++ )
                            {
                                if( self.housemodes()[i].configs[j].alarms()[k].uuid===uuid )
                                {
                                    //delete item
                                    self.housemodes()[i].configs[j].alarms.splice(k,1);
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    };

    //update device items (ie after drag n' drop)
    self.updateDeviceItems = function()
    {
        //make sure all items in housemode lists are trashable
        $('.housemode-device-list > .device-list-item').each(function() {
            //add trashable property
            $(this).addClass('security-device-item-trash');

            //add draggable property
            $(this).addClass('device-list-item-draggable');

            //add draggable possibility
            $(this).draggable({
                opacity: 0.7,
                cursor: 'move',
                revert: 'invalid',
                helper: 'clone',
                appendTo: $('#contentarea'),
                containment: $('#contentarea'),
                zIndex: 10000
            });
        });

        //set all housemode items trashable
        self.makeDeviceItemsTrashable();
    };

    //update device items (ie after drag n' drop)
    self.updateAlarmItems = function()
    {
        //make sure all items in housemode lists are trashable
        $('.housemode-alarm-list > .device-list-item').each(function() {
            //add trashable flag
            $(this).addClass('security-alarm-item-trash');

            //add draggable property
            $(this).addClass('device-list-item-draggable');

            //add draggable possibility
            $(this).draggable({
                opacity: 0.7,
                cursor: 'move',
                revert: 'invalid',
                helper: 'clone',
                appendTo: $('#contentarea'),
                containment: $('#contentarea'),
                zIndex: 10000
            });
        });

        //set all housemode items trashable
        self.makeAlarmItemsTrashable();
    };

    //enable drag and drop
    self.enableDragAndDrop = function()
    {
        //EDITOR ITEMS
        //droppable device areas
        $('.housemode-device-list').each(function() {
            $(this).droppable({
                accept: '.security-device-item',
                activate: function(event, ui) {
                    $(this).addClass('housemode-list-active');
                    $(this).find('div.device-list-item').hide();
                },
                deactivate: function(event, ui) {
                    $(this).removeClass('housemode-list-active');
                    $(this).find('div.device-list-item').show();
                },
                over: function(event, ui) {
                    $(this).addClass('housemode-list-hover');
                },
                out: function(event, ui) {
                    $(this).removeClass('housemode-list-hover');
                },
                drop: function(event, ui) {
                    //update style
                    $(this).removeClass('housemode-list-hover');

                    //handle dropped item
                    var housemode = $(this).data('hm');
                    var zone = $(this).data('zn');
                    var uuid = ui.draggable.attr('data-uuid');
                    self.addDroppableItem('device', housemode, zone, uuid);

                    //add missing class of brand new item (the clone)
                    //delay this action to make sure item is completely loaded in container
                    setTimeout(function() {
                        self.updateDeviceItems();
                    }, 250);
                }
            });
        });

        //droppable alarm areas
        $('.housemode-alarm-list').each(function() {
            $(this).droppable({
                accept: '.security-alarm-item',
                activate: function(event, ui) {
                    $(this).addClass('housemode-list-active');
                    $(this).find('div.device-list-item').hide();
                },
                deactivate: function(event, ui) {
                    $(this).removeClass('housemode-list-active');
                    $(this).find('div.device-list-item').show();
                },
                over: function(event, ui) {
                    $(this).addClass('housemode-list-hover');
                },
                out: function(event, ui) {
                    $(this).removeClass('housemode-list-hover');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('housemode-list-hover');

                    //handle dropped item
                    var housemode = $(this).data('hm');
                    var zone = $(this).data('zn');
                    var uuid = ui.draggable.attr('data-uuid');
                    self.addDroppableItem('alarm', housemode, zone, uuid);

                    //add missing class of brand new item (the clone)
                    //delay this action to make sure item is completely loaded in container
                    setTimeout(function() {
                        self.updateAlarmItems();
                    }, 250);
                }
            });
        });

        //items draggable (device and alarm items)
        $('.device-list-item').each(function() {
            $(this).addClass('device-list-item-draggable');
            $(this).draggable({
                opacity: 0.7,
                cursor: 'move',
                revert: 'invalid',
                helper: 'clone',
                appendTo: $('#contentarea'),
                containment: $('#contentarea'),
                zIndex: 10000
            });
        });

        //HOUSEMODE ITEMS
        self.makeDeviceItemsTrashable();
        self.makeAlarmItemsTrashable();

        self.updateDeviceItems();
        self.updateAlarmItems();

        //SELECT DELAY
        //enable select elements
        $('#securityTable').find('select').removeAttr('disabled').css('cursor', 'pointer');
    };

    //make device items trashables
    self.makeDeviceItemsTrashable = function()
    {
        //trash for device items
        $('.security-device-list').each(function() {
            $(this).droppable({
                accept: '.security-device-item-trash',
                activate: function(event, ui) {
                    $(this).addClass('device-list-active');
                    $(this).find('div.device-list-item').hide();
                },
                deactivate: function(event, ui) {
                    $(this).removeClass('device-list-active');
                    $(this).find('div.device-list-item').show();
                },
                over: function(event, ui) {
                    $(this).addClass('device-list-hover');
                },
                out: function(event, ui) {
                    $(this).removeClass('device-list-hover');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('device-list-hover');

                    //handle dropped item
                    var housemode = ui.draggable.parent().data('hm');
                    var zone = ui.draggable.parent().data('zn');
                    var uuid = ui.draggable.attr('data-uuid');
                    self.removeDroppableItem('device', housemode, zone, uuid);
                }
            });
        });
    };

    //make alarm items trashable
    self.makeAlarmItemsTrashable = function()
    {
        //trash for alarm items
        $('.security-alarm-list').each(function() {
            $(this).droppable({
                accept: '.security-alarm-item-trash',
                activate: function(event, ui) {
                    $(this).addClass('device-list-active');
                    $(this).find('div.device-list-item').hide();
                },
                deactivate: function(event, ui) {
                    $(this).removeClass('device-list-active');
                    $(this).find('div.device-list-item').show();
                },
                over: function(event, ui) {
                    $(this).addClass('device-list-hover');
                },
                out: function(event, ui) {
                    $(this).removeClass('device-list-hover');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('device-list-hover');

                    //handle dropped item
                    var housemode = ui.draggable.parent().data('hm');
                    var zone = ui.draggable.parent().data('zn');
                    var uuid = ui.draggable.attr('data-uuid');
                    self.removeDroppableItem('alarm', housemode, zone, uuid);
                }
            });
        });
    };

    //disable drag and drop
    self.disableDragAndDrop = function()
    {
        //destroy draggable and droppable
        $('.housemode-device-list').each(function() {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });
        $('.housemode-alarm-list').each(function() {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });
        $('.device-list-item').each(function() {
            $(this).removeClass('device-list-item-draggable');
            if( $(this).hasClass('ui-draggable') )
            {
                $(this).draggable('destroy');
            }
        });
        $('.security-device-list').each(function() {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });
        $('.security-alarm-list').each(function() {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });

        //disable select elements
        $('#securityTable').find('select').attr('disabled', 'disabled').css('cursor', 'auto');
    };

    //triggerable devices
    self.triggerableDevices = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.name && device.name.length>0 )
            {
                //only keep binary sensors
                if( device.devicetype=='binarysensor' )
                {
                    output.push({uuid:device.uuid, name:device.name, devicetype:device.devicetype});
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
            if( device.name && device.name.length>0 )
            {
                //only keep switch (maybe scenarii too?)
                if( device.devicetype=='switch' )
                {
                    output.push({uuid:device.uuid, name:device.name, devicetype:device.devicetype});
                }
            }
        }

        //append alert virtual devices
        if( self.alertEmailConfigured() )
        {
            output.push({uuid:'email', name:'Email', devicetype:'alertemail'});
        }
        if( self.alertTwitterConfigured() )
        {
            output.push({uuid:'twitter', name:'Twitter', devicetype:'alerttwitter'});
        }
        if( self.alertSmsConfigured() )
        {
            output.push({uuid:'sms', name:'SMS', devicetype:'alertsms'});
        }
        if( self.alertPushConfigured() )
        {
            output.push({uuid:'push', name:'Push', devicetype:'alertpush'});
        }
        
        return output;
    });

    //==================
    //GENERAL
    //==================
   
    //init security application
    self.init = function()
    {
        //get security controller uuid
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=="securitycontroller" )
            {
                self.securityController = self.agocontrol.devices()[i].uuid;
                self.loadSecurityConfig();
            }
            else if( self.agocontrol.devices()[i].devicetype=="alertcontroller" )
            {
                self.alertController = self.agocontrol.devices()[i].uuid;
                self.loadAlertConfig();
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
                        configs[zoneIdx[zoneMap[housemode][i].zone]].alarms.push(device);
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
                        configs[zoneIdx[zoneMap[housemode][i].zone]].devices.push(device);
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
        //virtual alert device
        if( uuid=='sms' || uuid=='email' || uuid=='push' || uuid=='twitter' )
        {
            //capitalize first letter
            var name = uuid.charAt(0).toUpperCase()+uuid.slice(1);
            return {'uuid':uuid, 'devicetype':'alert'+uuid, 'name':name};
        }

        //real device
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
        var content = {};
        content.command = "getconfig";
        content.uuid = self.securityController;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( res.config )
                {
                    self.zoneMap(res.config);
                    self.currentHousemode(res.housemode);
                    self.armedMessage(res.armedMessage);
                    self.disarmedMessage(res.disarmedMessage);
                    self.defaultHousemode(res.defaultHousemode);
                    self.initialized = true;
                }
            });
    };

    //get alert config
    this.loadAlertConfig = function()
    {
        var content = {};
        content.command = "status";
        content.uuid = self.alertController;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.alertEmailConfigured(res.mail.configured);
                self.alertSmsConfigured(res.sms.configured);
                self.alertTwitterConfigured(res.twitter.configured);
                self.alertPushConfigured(res.push.configured);
            });
    };

    //restore config from agosecurity
    self.restoreConfig = function()
    {
        //block ui
        self.agocontrol.block($('#securityTable'));

        //load config
        self.loadSecurityConfig();

        //unblock ui
        self.agocontrol.unblock($('#securityTable'));
    };

    //save security configuration
    // /!\ need pin
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
                    'devices' : [],
                    'alarms' : []
                };

                for( var k=0; k<self.housemodes()[i].configs[j].devices().length; k++ )
                {
                    config.devices.push(self.housemodes()[i].configs[j].devices()[k].uuid);
                }

                for( var k=0; k<self.housemodes()[i].configs[j].alarms().length; k++ )
                {
                    config.alarms.push(self.housemodes()[i].configs[j].alarms()[k].uuid);
                }

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
            .then(function(res) {
                notif.success('Housemode changed successfully');
            })
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
                    self.closePinPanel();
                    if( self.pinCallback )
                    {
                        self.pinCallback();
                        self.pinCallback = null;
                    }

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
            $('#pinPanel').addClass('active');
        }
    };

    //close pin panel
    self.closePinPanel = function()
    {
        //just hide modal
        $('#pinPanel').removeClass('active');
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
    //HOUSEMODES TAB
    //==================

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
        if( self.editorOpened )
        {
            notif.warning('Please stop edition before delete housemode');
            return;
        }

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
    };

    //delete house mode
    self.delHousemode = function(housemode)
    {
        if( self.editorOpened )
        {
            notif.warning('Please stop edition before delete housemode');
            return;
        }

        for( var i=0; i<self.housemodes().length; i++ )
        {
            if( self.housemodes()[i].name==housemode )
            {
                self.housemodes.splice(i, 1);
                break;
            }
        }
    };

    //start edit housemode
    self.startEditHousemode = function(housemode)
    {
        if( self.editorOpened )
        {
            notif.warning('Editor already opened');
            return;
        }

        //hide all housemodes except edited one
        $('#housemodesTable tr').each(function() {
            if( $(this).attr('id')!=housemode )
            {
                //$(this).css('display', 'none');
                $(this).hide();
            }
        });

        //add dummy class to device and alarm items to allow dropping them in specific area
        $('.security-alarm-list > .device-list-item').each(function() {
            $(this).addClass('security-alarm-item');
        }); 
        $('.security-device-list > .device-list-item').each(function() {
            $(this).addClass('security-device-item');
        });

        //show editor
        self.openEditor();
    };

    //stop edit housemode
    self.stopEditHousemode = function()
    {
        if( self.editorOpened )
        {
            //close editor
            self.closeEditor();

            //display all housemodes
            $('#housemodesTable tr').each(function() {
                $(this).show();
            });

            //save config
            self.checkPin(self.saveConfig);
        }
    };

    //cancel edit housemode
    self.cancelEditHousemode = function()
    {
        if( self.editorOpened )
        {
            //close editor
            self.closeEditor();

            //display all housemodes
            $('#housemodesTable tr').each(function() {
                $(this).show();
            });

            //restore old config
            self.restoreConfig();
        }
    };

    //==================
    //ZONES
    //==================

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
        if( self.editorOpened )
        {
            notif.warning('Please stop edition before delete housemode');
            return;
        }

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
    };

    //delete zone
    self.delZone = function(zone)
    {
        if( self.editorOpened )
        {
            notif.warning('Please stop edition before delete housemode');
            return;
        }

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
        else if( item.devicetype.indexOf('alert')===0 )
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

