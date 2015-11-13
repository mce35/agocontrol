/**
 * Model class
 */
function Processes(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.processes = ko.observableArray([]);
    self.grid = new ko.agoGrid.viewModel({
        data: self.processes,
        pageSize: 50,
        columns: [
            {headerText:'Process', rowText:'name'},
            {headerText:'Is running', rowText:'running'},
            {headerText:'CPU usage', rowText:'cpu'},
            {headerText:'Memory', rowText:'memRes'}
        ],
        rowTemplate: 'rowTemplate'
    });

    //get processes status
    self.getStatus = function()
    {
        var content = {};
        content.uuid = self.agocontrol.systemController;
        content.command = 'getprocesslist';
        self.agocontrol.sendCommand(content, function(res) {
            var procs = [];
            for( var name in res.result.data )
            {
                var proc = {};
                proc.name = name;
                proc.running = res.result.data[name].running;
                proc.cpu = (Math.round((res.result.data[name].currentStats.ucpu + res.result.data[name].currentStats.scpu)*10)/10)+'%';
                proc.memVirt = res.result.data[name].currentStats.vsize;
                proc.memRes = res.result.data[name].currentStats.rss;
                proc.monitored = res.result.data[name].monitored;
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
function init_template(path, params, agocontrol)
{
    var model = new Processes(agocontrol);
    return model;
}

/**
 * Reset template
 */
function reset_template(model)
{
    if( model )
    {
        window.clearInterval(model.updateInterval);
    }
}
