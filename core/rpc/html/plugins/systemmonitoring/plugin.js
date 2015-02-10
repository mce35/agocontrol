/**
 * System monitoring plugin
 */
function systemMonitoringConfig(devices)
{
    //members
    var self = this;
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
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    var procs = [];
                    var monitored = [];
                    for( var name in res.result.processes )
                    {
                        procs.push(name);
                        if( res.result.processes[name].monitored )
                        {
                            monitored.push(name);
                        }
                    }
                    self.processes(procs);
                    self.monitoredProcesses(monitored);
                    self.memoryThreshold(res.result.memoryThreshold);
                }
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
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('Monitored process list saved successfully');
                    }
                    else
                    {
                        notif.error('Unable to save monitored process list.');
                    }
                }
                else
                {
                    notif.fatal('Unable to save monitored process list. Internal error');
                }
            });
        }
    };

    //save new memory threshold
    self.setMemoryThreshold = function() {
        if( self.controllerUuid )
        {
            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setmemorythreshold';
            content.threshold = self.memoryThreshold();
            sendCommand(content, function(res) {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('Memory threshold saved successfully');
                    }
                    else
                    {
                        notif.error('Unable to save memory threshold.');
                        self.memoryThreshold(res.result.old);
                    }
                }
                else
                {
                    notif.fatal('Unable to save memory threshold. Internal error');
                }
            });
        }
    };

    //init
    self.getStatus();
} 


/**
 * Entry point: mandatory!
 */
function init_plugin(fromDashboard)
{
    ko.bindingHandlers.jqTabs = {
        init: function(element, valueAccessor) {
            var options = valueAccessor() || {};
            setTimeout( function() { $(element).tabs(options); }, 0);
        }
    };

    var model = new systemMonitoringConfig(deviceMap);

    model.mainTemplate = function() {
        return templatePath + 'systemMonitoringConfig';
    }.bind(model);

    return model;
}

