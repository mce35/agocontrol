/**
 * Datalogger plugin
 */
function dataloggerConfig(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
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
    self.timestampToHRTime = function(ts)
    {
        if( ts==0 )
            return 'X';
        var date = new Date(ts * 1000);
        return ''+date.getFullYear()+'/'+(date.getMonth()+1)+'/'+date.getDate()+' '+date.getHours()+':'+date.getMinutes()+':'+date.getSeconds();
    }

    //return human readable size
    //http://stackoverflow.com/a/20463021/3333386
    self.sizeToHRSize = function (a,b,c,d,e)
    {
        return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes')
    }

    //get device name. Always returns something
    self.getDeviceName = function(uuid)
    {
        var name = '';
        var type = '';
        var found = false;
        for( var i=0; i<devices.length; i++ )
        {
            //if( devices[i].internalid==obj.internalid )
            if( devices[i].uuid==uuid )
            {
                if( devices[i].name.length!=0 )
                {
                    name = devices[i].name;
                }
                else
                {
                    name = devices[i].internalid;
                }
                type = devices[i].devicetype.replace('sensor', '');
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
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='dataloggercontroller' )
            {
                self.controllerUuid = devices[i].uuid;
            }
            else if( devices[i].devicetype.match(/sensor$/) && devices[i].devicetype.indexOf('gps')==-1 && devices[i].devicetype.indexOf('binary')==-1)
            {
                var device = {'uuid':devices[i].uuid, 'name':self.getDeviceName(devices[i].uuid)};
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
            self.agocontrol.sendCommand(content)
                .then(function(res) {
                    var data = res.data;
                    //update multigraphs
                    self.multigraphs.removeAll();
                    for( var i=0; i<data.multigraphs.length; i++ )
                    {
                        var uuids = [];
                        for( var j=0; j<data.multigraphs[i].uuids.length; j++ )
                        {
                            uuids.push({'uuid':data.multigraphs[i].uuids[j], 'name':self.getDeviceName(data.multigraphs[i].uuids[j])});
                        }
                        self.multigraphs.push({'name':data.multigraphs[i].name, 'uuids':uuids});
                    }
    
                    //update sql,rrd state
                    self.dataLogging(data.dataLogging);
                    self.gpsLogging(data.gpsLogging);
                    self.rrdLogging(data.rrdLogging);

                    //update database infos
                    self.databaseSize(self.sizeToHRSize(data.database.size));
                    self.dataCount(data.database.infos.data_count);
                    self.dataStart(self.timestampToHRTime(data.database.infos.data_start));
                    self.dataEnd(self.timestampToHRTime(data.database.infos.data_end));
                    self.positionCount(data.database.infos.position_count);
                    self.positionStart(self.timestampToHRTime(data.database.infos.position_start));
                    self.positionEnd(self.timestampToHRTime(data.database.infos.position_end));
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
            self.agocontrol.sendCommand(content)
                .then(function(res) {
                    notif.success(res.message);
                    self.getStatus();
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
                self.agocontrol.sendCommand(content)
                    .then(function(res) {
                        notif.success(res.message);
                        self.getStatus();
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
            self.agocontrol.sendCommand(content)
                .then(function(res) {
                    notif.success("Logging flags saved");
                    //no need to get status here
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
                //TODO doesn't work until sendcommand oldstyleCallback parameter is not removed !
                self.agocontrol.sendCommand(content, 30)
                    .then(function(res) {
                        notif.success("Table purged successfully");
                        self.getStatus();
                    });
            }
        }
    };
    self.purgeDataTable = function()
    {
        self.purgeDatabaseTable('data');
    };
    self.purgePositionTable = function()
    {
        self.purgeDatabaseTable('position');
    };

    self.getStatus();
} 

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    ko.bindingHandlers.jqTabs = {
        init: function(element, valueAccessor) {
            var options = valueAccessor() || {};
            setTimeout( function() { $(element).tabs(options); }, 0);
        }
    };

    var model = new dataloggerConfig(agocontrol.devices(), agocontrol);
    return model;
}

