/**
 * Model class
 * 
 * @returns {systemStatus}
 */
function systemStatus() {
    this.hasNavigation = ko.observable(false);
    var self = this;
    self.processes = ko.observableArray([]);
    self.updateInterval = null;
    self.systemController = null;
    self.interval = null;

    //get systemcontroller uuid
    self.getUuid = function()
    {
	if( deviceMap!==undefined )
        {
            for( var i=0; i<deviceMap.length; i++ )
            {
                if( deviceMap[i].devicetype=='systemcontroller' )
                {
                    self.systemController = deviceMap[i].uuid;
                }
            }
        }

        if( self.systemController!=null )
        {
            self.updateInterval = window.setInterval(self.getStatus, 10000);
            self.getStatus();
            window.clearInterval(self.interval);
        }
    }

    //return human readable size
    //http://stackoverflow.com/a/20463021/3333386
    self.sizeToHRSize = function (a,b,c,d,e) {
        return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes')
    }

    //get processes status
    self.getStatus = function()
    {
        var content = {};
        content.uuid = systemController;
        content.command = 'getprocesslist';
        sendCommand(content, function(res) {
            var procs = [];
            for( var name in res.result )
            {
                var proc = {};
                proc.name = name;
                proc.running = res.result[name].running;
                proc.cpu = (Math.round((res.result[name].currentStats.ucpu + res.result[name].currentStats.scpu)*10)/10)+'%';
                proc.memVirt = res.result[name].currentStats.vsize;
                proc.memRes = res.result[name].currentStats.rss;
                procs.push(proc);
            }
            self.processes(procs);
        });
    };

    //launch autorefresh and get current status
    self.interval = window.setInterval(self.getUuid, 1000);
}

/**
 * Initalizes the model
 */
function init_systemStatus() {
    model = new systemStatus();

    model.mainTemplate = function() {
        return "systemStatus";
    }.bind(model);

    model.navigation = function() {
        return "";
    }.bind(model);

    ko.applyBindings(model);
}

