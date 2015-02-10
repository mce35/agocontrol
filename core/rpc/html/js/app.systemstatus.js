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

    //return human readable size
    //http://stackoverflow.com/a/20463021/3333386
    self.sizeToHRSize = function (a,b,c,d,e) {
        return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes')
    }

    //get processes status
    self.getStatus = function()
    {
        var content = {};
        content.uuid = self.agocontrol.systemController;
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
    self.updateInterval = window.setInterval(self.getStatus, 10000);
    self.getStatus();
}

/**
 * Initalizes the model
 */
function init_systemStatus() {
    model = new systemStatus();
}

