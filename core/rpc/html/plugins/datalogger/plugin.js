/**
 * Datalogger plugin
 */
function dataloggerConfig(deviceMap) {
    //members
    var self = this;
    self.controllerUuid = null;
    self.multigraphs = ko.observableArray([]);
    self.sensors = ko.observableArray([]);
    self.selectedSensors = ko.observableArray([]);
    self.multigraphPeriod = ko.observable(12);
    self.selectedMultigraphToDel = ko.observable();
    self.selectedMultigraphSensors = ko.computed(function() {
        var sensors = [];
        if( self.selectedMultigraphToDel() )
        {
            for( var i=0; i<self.selectedMultigraphToDel().uuids.length; i++ )
            {
                sensors.push(self.selectedMultigraphToDel().uuids[i].name);
            }
        }
        return sensors;
    });

    //get device name. Always returns something
    self.getDeviceName = function(uuid) {
        var name = '';
        var type = '';
        var found = false;
        for( var i=0; i<deviceMap.length; i++ )
        {
            //if( deviceMap[i].internalid==obj.internalid )
            if( deviceMap[i].uuid==uuid )
            {
                if( deviceMap[i].name.length!=0 )
                {
                    name = deviceMap[i].name;
                }
                else
                {
                    name = deviceMap[i].internalid;
                }
                type = deviceMap[i].devicetype.replace('sensor', '');
                found = true;
                break;
            }
        }

        if( !found )
        {
            //nothing found
            name = uuid;
        }

        name += ' ('+type+')';
        return name;
    };

    //datalogger controller and sensors uuids
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='dataloggercontroller' )
            {
                self.controllerUuid = deviceMap[i].uuid;
            }
            else if( deviceMap[i].devicetype.match(/sensor$/) && deviceMap[i].devicetype.indexOf('gps')==-1 && deviceMap[i].devicetype.indexOf('binary')==-1)
            {
                var device = {'uuid':deviceMap[i].uuid, 'name':self.getDeviceName(deviceMap[i].uuid)};
                self.sensors.push(device);
            }
        }
    }

    //get multigraphs
    self.getMultigraphs = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getmultigraphs';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    self.multigraphs.removeAll();
                    //append other informations
                    for( var i=0; i<res.result.multigraphs.length; i++ )
                    {
                        var uuids = [];
                        for( var j=0; j<res.result.multigraphs[i].uuids.length; j++ )
                        {
                            uuids.push({'uuid':res.result.multigraphs[i].uuids[j], 'name':self.getDeviceName(res.result.multigraphs[i].uuids[j])});
                        }
                        self.multigraphs.push({'name':res.result.multigraphs[i].name, 'uuids':uuids});
                    }
                }
            });
        }
    };

    //add multigraph
    self.addMultigraph = function()
    {
        if( self.controllerUuid )
        {
            var uuids = [];
            for( var i=0; i<self.selectedSensors().length; i++ )
            {
                uuids.push(self.selectedSensors()[i].uuid);
            }
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'addmultigraph';
            content.uuids = uuids;
            content.period = self.multigraphPeriod();
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success(res.result.msg);
                        self.getMultigraphs();
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
            });
        }
    };

    //delete multigraph
    self.deleteMultigraph = function()
    {
        if( self.controllerUuid )
        {
            if( confirm('Delete selected multigraph?') )
            {
                var content = {};
                content.uuid = self.controllerUuid;
                content.command = 'deletemultigraph';
                content.multigraph = self.selectedMultigraphToDel().name;
                sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                    {
                        if( res.result.error==0 )
                        {
                            notif.success(res.result.msg);
                            self.getMultigraphs();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                });
            }
        }
    };

    self.getMultigraphs();
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

    model = new dataloggerConfig(deviceMap);
    template = 'dataloggerConfig';

    model.mainTemplate = function() {
        return templatePath + template;
    }.bind(model);

    return model;
}

