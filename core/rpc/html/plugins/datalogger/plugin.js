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
    self.dataLogging = ko.observable(0);
    self.gpsLogging = ko.observable(0);
    self.rrdLogging = ko.observable(0);
    self.dataCount = ko.observable(0);
    self.dataStart = ko.observable(0);
    self.dataEnd = ko.observable(0);
    self.positionCount = ko.observable(0);
    self.positionStart = ko.observable(0);
    self.positionEnd = ko.observable(0);
    self.databaseSize = ko.observable(0);

    //return human readable time
    self.timestampToHRTime = function(ts) {
        if( ts==0 )
            return 'X';
        var date = new Date(ts * 1000);
        return ''+date.getFullYear()+'/'+(date.getMonth()+1)+'/'+date.getDate()+' '+date.getHours()+':'+date.getMinutes()+':'+date.getSeconds();
    }

    //return human readable size
    //http://stackoverflow.com/a/20463021/3333386
    self.sizeToHRSize = function (a,b,c,d,e) {
        return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes')
    }

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

    //get status
    self.getStatus = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getstatus';
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    //update multigraphs
                    self.multigraphs.removeAll();
                    for( var i=0; i<res.result.multigraphs.length; i++ )
                    {
                        var uuids = [];
                        for( var j=0; j<res.result.multigraphs[i].uuids.length; j++ )
                        {
                            uuids.push({'uuid':res.result.multigraphs[i].uuids[j], 'name':self.getDeviceName(res.result.multigraphs[i].uuids[j])});
                        }
                        self.multigraphs.push({'name':res.result.multigraphs[i].name, 'uuids':uuids});
                    }

                    //update sql,rrd state
                    self.dataLogging(res.result.dataLogging);
                    self.gpsLogging(res.result.gpsLogging);
                    self.rrdLogging(res.result.rrdLogging);

                    //update database infos
                    self.databaseSize(self.sizeToHRSize(res.result.database.size));
                    self.dataCount(res.result.database.infos.data_count);
                    self.dataStart(self.timestampToHRTime(res.result.database.infos.data_start));
                    self.dataEnd(self.timestampToHRTime(res.result.database.infos.data_end));
                    self.positionCount(res.result.database.infos.position_count);
                    self.positionStart(self.timestampToHRTime(res.result.database.infos.position_start));
                    self.positionEnd(self.timestampToHRTime(res.result.database.infos.position_end));
                }
                else
                {
                    //TODO not responding
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
                        self.getStatus();
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    //TODO not responding
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
                            self.getStatus();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                    else
                    {
                        //TODO not responding
                    }
                });
            }
        }
    };

    //save enabled modules
    self.saveEnabledModules = function()
    {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setenabledmodules';
            content.dataLogging = self.dataLogging();
            content.gpsLogging = self.gpsLogging();
            content.rrdLogging = self.rrdLogging();
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success("Logging flags saved");
                        //no need to get status here
                    }
                    else
                    {
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    //TODO not responding
                }
            });
        }
    };

    //purge database table
    self.purgeDatabaseTable = function(tablename)
    {
        if( self.controllerUuid )
        {
            if( confirm('Really purge table? Operation is irreversible!') )
            {
                notif.info('Purging table "'+tablename+'", please wait...');
                var content = {};
                content.uuid = self.controllerUuid;
                content.command = 'purgetable';
                content.table = tablename;
                sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                    {
                        if( res.result.error==0 )
                        {
                            notif.success("Table purged successfully");
                            self.getStatus();
                        }
                        else
                        {
                            notif.error(res.result.msg);
                        }
                    }
                    else
                    {
                        //TODO not responding
                    }
                }, 30); //increase timeout because table purge can be long
            }
        }
    };
    self.purgeDataTable = function() {
        self.purgeDatabaseTable('data');
    };
    self.purgePositionTable = function() {
        self.purgeDatabaseTable('position');
    };

    self.getStatus();
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

