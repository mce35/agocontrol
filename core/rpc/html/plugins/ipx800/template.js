/**
 * Ipx plugin
 */
function ipxConfig(deviceMap) {
    //members
    var self = this;
    self.controllerUuid = null;
    self.boards = ko.observableArray([]);
    self.boardIp = ko.observable();
    self.devswitch = ko.observable(true);
    self.devdrapes = ko.observable(false);
    self.selectedOutputPin1 = ko.observable();
    self.selectedOutputPin2 = ko.observable();
    self.selectedAnalogPin1 = ko.observable();
    self.selectedCounterPin1 = ko.observable();
    self.selectedDigitalPin1 = ko.observable();
    self.selectedBoardUuid = null;
    self.selectedBoard = ko.observable();
    self.selectedOutputType = ko.observable();
    self.selectedOutputType.subscribe(function(newVal) {
        if (newVal == "switch") {
            self.devswitch(true);
            self.devdrapes(false);
        } else if (newVal == "drapes") {
            self.devswitch(false);
            self.devdrapes(true);
        }
    });
    self.selectedAnalogType = ko.observable();
    self.allDevices = ko.observableArray([]);
    self.outputDevices = ko.observableArray([]);
    self.digitalDevices = ko.observableArray([]);
    self.binaryDevices = ko.observableArray([]);
    self.analogDevices = ko.observableArray([]);
    self.counterDevices = ko.observableArray([]);
    self.selectedDeviceToForce = ko.observable();
    self.selectedDeviceToDelete = ko.observable();
    self.selectedCounterToReset = ko.observable();
    self.selectedDeviceState = ko.observable();
    self.selectedLinkBinary = ko.observable();
    self.selectedLinkOutput = ko.observable();
    self.selectedLinkToDelete = ko.observable();
    self.links = ko.observableArray([]);

    //ipx controller uuid and boards
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='ipx800controller' )
            {
                self.controllerUuid = deviceMap[i].uuid;
            }
        }
    }
    
    //get device name. Always returns something
    self.getDeviceName = function(obj) {
        var name = '';
        var found = false;
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].internalid==obj.internalid )
            {
                if( deviceMap[i].name.length!=0 )
                {
                    name = deviceMap[i].name;
                }
                else
                {
                    name = obj.internalid;
                }
                found = true;
                break;
            }
        }

        if( !found )
        {
            //nothing found
            name = obj.internalid;
        }

        name += '('+obj.type;
        if( obj.inputs )
            name += '['+obj.inputs+']';
        name += ')';
        return name;
    };

    //get link name. Always returns something
    self.getLinkName = function(obj) {
        var outputName = self.getDeviceName(obj.output);
        var binaryName = self.getDeviceName(obj.binary);
        return binaryName+' => '+outputName;
    };

    //get board status
    self.getBoardStatus = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'status';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error===0 )
                    {
                        //console.log('STATUS res:');
                        //console.log(res);
                        $('#currentoutputs').html(res.result.status.outputs);
                        $('#currentanalogs').html(res.result.status.analogs);
                        $('#currentcounters').html(res.result.status.counters);
                        $('#currentdigitals').html(res.result.status.digitals);

                        //console.log('BOARD DEVICES:');
                        //console.log(res.result.devices);
                        //clear previous content
                        self.outputDevices.removeAll();
                        self.digitalDevices.removeAll();
                        self.binaryDevices.removeAll();
                        self.analogDevices.removeAll();
                        self.counterDevices.removeAll();
                        self.allDevices.removeAll();
                        self.links.removeAll();

                        //append new one
                        var i=0;
                        for( i=0; i<res.result.devices.outputs.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.outputs[i].internalid, 'name':self.getDeviceName(res.result.devices.outputs[i])};
                            self.outputDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.digitals.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.digitals[i].internalid, 'name':self.getDeviceName(res.result.devices.digitals[i])};
                            self.digitalDevices.push(dev);
                            if( res.result.devices.digitals[i].type=='dbinary' )
                            {
                                self.binaryDevices.push(dev);
                            }
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.analogs.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.analogs[i].internalid, 'name':self.getDeviceName(res.result.devices.analogs[i])};
                            self.analogDevices.push(dev);
                            if( res.result.devices.analogs[i].type=='abinary' )
                            {
                                self.binaryDevices.push(dev);
                            }
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.counters.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.counters[i].internalid, 'name':self.getDeviceName(res.result.devices.counters[i])};
                            self.counterDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.links.length; i++ )
                        {
                            var link = {'name':self.getLinkName(res.result.links[i]), 'output':res.result.links[i].output, 'binary':res.result.links[i].binary};
                            self.links.push(link);
                        }
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr');
                }
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //update UI
    self.updateUi = function()
    {
        self.getBoardStatus();
    };

    //return board uuid (or null if not found)
    self.getBoardUuid = function(internalid)
    {
        if( internalid )
        {
            for( var i=0; i<deviceMap.length; i++ )
            {
                if( deviceMap[i].devicetype=='ipx800v3board' && deviceMap[i].internalid==internalid)
                {
                    return deviceMap[i].uuid;
                }
            }
        }
        return null;
    };

    self.selectedBoard.subscribe(function(newVal) {
        //update selected ipx board uuid
        self.selectedBoardUuid = self.getBoardUuid(newVal);
        if( self.selectedBoardUuid )
        {
            self.updateUi();
        }
    });

    //add a device
    self.addDevice = function(content, callback)
    {
        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.error===0 )
                {
                    notif.success(res.result.msg);
                    if( callback!==undefined )
                        callback();
                }
                else
                {
                    notif.error(res.result.msg);
                }
            }
            else
            {
                notif.fatal('#nr');
            }
        });
    };

    //get boards
    self.getBoards = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getboards';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    self.boards.removeAll();
                    self.boards(res.result.boards);
                    //console.log("BOARDS:");
                    //console.log(res.result.boards);
    
                    //select first board
                    if( self.boards().length>0 )
                    {
                        //update selectedBoard
                        self.selectedBoard(self.boards()[0]);
                        //no need to updateUi, it's performed automatically when selectedBoard is updated
                    }
                }
            });
        }
    }

    //get devices
    self.getDevices = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'getdevices';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    //console.log('BOARD DEVICES:');
                    //console.log(res.result.devices);
                    
                    if( res.result.error===0 )
                    {
                        //clear previous content
                        self.outputDevices.removeAll();
                        self.digitalDevices.removeAll();
                        self.analogDevices.removeAll();
                        self.counterDevices.removeAll();
                        self.allDevices.removeAll();

                        //append new one
                        var i=0;
                        for( i=0; i<res.result.devices.outputs.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.outputs[i], 'name':self.getDeviceName(res.result.devices.outputs[i])};
                            self.outputDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.digitals.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.digitals[i], 'name':self.getDeviceName(res.result.devices.digitals[i])};
                            self.digitalDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.analogs.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.analogs[i], 'name':self.getDeviceName(res.result.devices.analogs[i])};
                            self.analogDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                        for( i=0; i<res.result.devices.counters.length; i++ )
                        {
                            var dev = {'internalid':res.result.devices.counters[i], 'name':self.getDeviceName(res.result.devices.counters[i])};
                            self.counterDevices.push(dev);
                            self.allDevices.push(dev);
                        }
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr');
                }
            });
        }
    };

    //add new IPX board
    self.addBoard = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'addboard';
            content.ip = self.boardIp();
            self.addDevice(content, function() {
                self.getBoards();
            });
        }
        else
        {
            notif.fatal('#nr');
        }
    };

    //add new output
    self.addOutput = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            if( self.selectedOutputType()=="switch" )
            {
                content.type = "oswitch";
                content.pin1 = self.selectedOutputPin1();
            }
            else if( self.selectedOutputType()=="drapes" )
            {
                content.type = "odrapes";
                content.pin1 = self.selectedOutputPin1();
                content.pin2 = self.selectedOutputPin2();
            }
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new analog
    self.addAnalog = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = "a"+self.selectedAnalogType();
            content.pin1 = self.selectedAnalogPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new counter
    self.addCounter = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = 'counter';
            content.pin1 = self.selectedCounterPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add new digital
    self.addDigital = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'adddevice';
            content.type = 'dbinary';
            content.pin1 = self.selectedDigitalPin1();
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //add link between digital binary and output
    self.addLink = function() {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'addlink';
            content.binary = self.selectedLinkBinary().internalid;
            content.output = self.selectedLinkOutput().internalid;
            self.addDevice(content, function() {
                self.updateUi();
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //delete link
    self.deleteLink = function() {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected link?') )
            {
                var content = {};
                content.uuid = self.selectedBoardUuid;
                content.output = self.selectedLinkToDelete().output.internalid;
                content.digital = self.selectedLinkToDelete().binary.internalid;
                content.command = 'deletelink';
                sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                    {
                        if( !res.result.error )
                        {
                            notif.success('Link deleted');
                            self.updateUi();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                    else
                    {
                        notif.error('Internal error');
                    }
                });
            }
        }
    };

    //force drape state
    self.forceState = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.device = self.selectedDeviceToForce().internalid;
            content.command = 'forcestate';
            content.state = self.selectedDeviceState();
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error===0 )
                        notif.success('State forced successfully');
                    else
                        notif.error(res.result.msg);
                }
                else
                {
                    notif.fatal('#nr');
                }
            });
        }
        else
        {
            notif.warning('Please select a board first');
        }
    };

    //all on
    self.allOn = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'allon';
            sendCommand(content);
        }
    };
    
    //all off
    self.allOff = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.command = 'alloff';
            sendCommand(content);
        }
    };

    //delete board
    self.deleteBoard = function()
    {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected board?') )
            {
                var content = {};
                content.uuid = self.controllerUuid;
                content.command = 'deleteboard';
                content.board = self.selectedBoardUuid;
                sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                    {
                        if( res.result.error==0 )
                        {
                            notif.success(res.result.msg);
                            self.getBoards();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                    else
                    {
                        notif.fatal('#nr');
                    }
                });
            }
        }
    };

    //delete device
    self.deleteDevice = function()
    {
        if( self.selectedBoardUuid )
        {
            if( confirm('Really delete selected device?') )
            {
                var content = {};
                content.uuid = self.selectedBoardUuid;
                content.device = self.selectedDeviceToDelete().internalid;
                content.command = 'deletedevice';
                sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                    {
                        if( !res.result.error )
                        {
                            notif.success('Device deleted');
                            self.updateUi();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                    else
                    {
                        notif.error('Internal error');
                    }
                });
            }
        }
    };

    //reset counter
    self.resetCounter = function()
    {
        if( self.selectedBoardUuid )
        {
            var content = {};
            content.uuid = self.selectedBoardUuid;
            content.device = self.selectedCounterToReset().internalid;
            content.command = 'reset';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( !res.result.error )
                    {
                        notif.success('Counter reseted');
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.error('Internal error');
                }
            });
        }
    };

    //get boards
    self.getBoards();
} 

/**
 * Entry point: mandatory!
 */
function init_plugin(fromDashboard)
{
    var model;
    var template;

    ko.bindingHandlers.jqTabs = {
        init: function(element, valueAccessor) {
        var options = valueAccessor() || {};
            setTimeout( function() { $(element).tabs(options); }, 0);
        }
    };

    model = new ipxConfig(deviceMap);
    template = 'ipxConfig';

    model.mainTemplate = function() {
        return templatePath + template;
    }.bind(model);

    return model;
}

