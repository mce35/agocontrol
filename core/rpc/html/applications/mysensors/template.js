/**
 * MySensors plugin
 */
function MySensors(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.mysensorsControllerUuid = null;
    self.port = ko.observable();
    self.devices = ko.observableArray();
    self.selectedRemoveDevice = ko.observable();
    self.selectedCountersDevice = ko.observable();
    self.counters = ko.observableArray([]);
    self.countersGrid = new ko.agoGrid.viewModel({
        data: self.counters,
        columns: [
            {headerText: 'Device', rowText:'device'},
            {headerText: 'Sent', rowText:'counter_sent'},
            {headerText: 'Failed', rowText:'counter_failed'},
            {headerText: 'Received', rowText:'counter_received'}
        ],
        rowTemplate: 'countersRowTemplate',
        pageSize: 25
    });

    //MySensor controller uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='mysensorscontroller' )
            {
                self.mysensorsControllerUuid = devices[i].uuid;
                break;
            }
        }
    }

    //set port
    self.setPort = function() {
        //first of all unfocus element to allow observable to save its value
        $('#setport').blur();
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'setport',
            port: self.port()
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.error==0 )
                {
                    notif.success('#sp');
                }
                else 
                {
                    //error occured
                    notif.error(res.result.msg);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get port
    self.getPort = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getport'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.port(res.result.port);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //reset all counters
    self.resetAllCounters = function() {
        if( confirm("Reset all counters?") )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'resetallcounters'
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //reset counters
    self.resetCounters = function() {
        if( confirm("Reset counters of selected device?") )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'resetcounters',
                device: self.selectedCountersDevice()
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get devices list
    self.getDevices = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getdevices'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.devices(res.result.devices);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //remove device
    self.removeDevice = function() {
        if( confirm('Delete device?') )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'remove',
                device: self.selectedRemoveDevice()
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('#ds');
                        //refresh devices list
                        self.getDevices();
                        self.getCounters();
                    }
                    else 
                    {
                        //error occured
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get counters
    self.getCounters = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getcounters'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                for( device in res.result.counters )
                {
                    self.counters.push(res.result.counters[device]);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //init ui
    self.getPort();
    self.getDevices();
    self.getCounters();
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new MySensors(agocontrol.devices(), agocontrol);
    return model;
}

