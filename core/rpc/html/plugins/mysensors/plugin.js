/**
 * MySensors plugin
 */
function mysensorsConfig(deviceMap) {
    //members
    var self = this;
    self.hasNavigation = ko.observable(false);
    self.mysensorsControllerUuid = null;
    self.port = ko.observable();

    //MySensor controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='mysensorscontroller' )
            {
                self.mysensorsControllerUuid = deviceMap[i].uuid;
                console.log('uuid='+self.mysensorsControllerUuid);
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

        sendCommand(content, function(res)
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

        sendCommand(content, function(res)
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

    //reset counters
    self.resetCounters = function() {
        if( confirm("Reset all counters?") )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'resetcounters'
            }
    
            sendCommand(content, function(res)
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

    //init ui
    self.getPort();
}

function mysensorsDashboard(deviceMap) {
    //members
    var self = this;
    self.hasNavigation = ko.observable(false);
    self.mysensorsControllerUuid = null;
    self.port = ko.observable();
    self.counters = ko.observableArray([]);

    //MySensor controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='mysensorscontroller' )
            {
                self.mysensorsControllerUuid = deviceMap[i].uuid;
                console.log('uuid='+self.mysensorsControllerUuid);
                break;
            }
        }
    }

    //get counters
    self.getCounters = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getcounters'
        }

        sendCommand(content, function(res)
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

    //get counters
    self.getCounters();
}

/**
 * Entry point: mandatory!
 */
function init_plugin(fromDashboard)
{
    var model;
    var template;
    if( fromDashboard )
    {
        model = new mysensorsDashboard(deviceMap);
        template = 'mysensorsDashboard';
    }
    else
    {
        model = new mysensorsConfig(deviceMap);
        template = 'mysensorsConfig';
    }
    model.mainTemplate = function() {
        return templatePath + template;
    }.bind(model);
    ko.applyBindings(model);
}

