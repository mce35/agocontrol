/**
 * System monitoring plugin
 */
function systemMonitoringConfig(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.controllerUuid = null;
    self.processes = ko.observableArray([]);
    self.monitoredProcesses = ko.observableArray([]);
    self.memoryThreshold = ko.observable(0);

    //controller uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='systemcontroller' )
            {
                self.controllerUuid = devices[i].uuid;
            }
        }
    }

    //get status
    self.getStatus = function() {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getstatus';
            self.agocontrol.sendCommand(content)
                .then(function(res) {
                    var procs = [];
                    var monitored = [];
                    for( var name in res.data.processes )
                    {
                        procs.push(name);
                        if( res.data.processes[name].monitored )
                        {
                            monitored.push(name);
                        }
                    }
                    self.processes(procs);
                    self.monitoredProcesses(monitored);
                    self.memoryThreshold(res.data.memoryThreshold);
                })
                .catch(function(error) {
                    notif.fatal('Unable to get monitored process list: ' +
                            getErrorMessage(error));
                });
        }
    };

    //save monitored process flag
    self.setMonitoredProcesses = function() {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setmonitoredprocesses';
            content.processes = self.monitoredProcesses();
            self.agocontrol.sendCommand(content)
                .then(function(res){
                    notif.success('Monitored process list saved successfully');
                })
                .catch(function(error) {
                    notif.fatal('Unable to save monitored process list: ' +
                            getErrorMessage(error));
                });
        }
    };

    //save new memory threshold
    self.setMemoryThreshold = function() {
        if( self.controllerUuid )
        {
            // normalize
            var v = parseInt(self.memoryThreshold(), 10);
            if(isNaN(v))
                v  = 0;

            self.memoryThreshold(v);

            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setmemorythreshold';
            content.threshold = self.memoryThreshold();

            self.agocontrol.sendCommand(content)
                .then(function(res){
                    notif.success('Memory threshold saved successfully');
                })
                .catch(function(error) {
                    notif.fatal('Unable to save memory threshold: ' +
                            getErrorMessage(error));
                    if(error.data && error.data.old)
                        self.memoryThreshold(error.data.old);
                });
        }
    };

    //init
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

    var model = new systemMonitoringConfig(agocontrol.devices(), agocontrol);
    return model;
}

