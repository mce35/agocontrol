/**
 * Model class
 * 
 * @returns {systemConfig}
 */
function SystemConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.inventory = ko.observable(JSON.stringify(self.agocontrol.inventory, undefined, 2));
    self.updateInterval = null;
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

    //==============
    //CONFIGURATION
    //==============
    self.configTree = ko.observableArray([]);
    self.configTreeGrid = new ko.agoGrid.viewModel({
        data: self.configTree,
        columns: [
            {headerText:'Parameter', rowText:'name'},
            {headerText:'Value', rowText:''},
            {headerText:'Action', rowText:''}
        ],
        rowTemplate: 'configTreeRowTemplate',
        pageSize: 100
    });

    //get config tree
    self.getConfigTree = function()
    {
        var content = {};
        content.command = 'getconfigtree';
        content.uuid = self.agocontrol.agoController;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( res.data.config )
                {
                    var configs = [];
                    for( var app in res.data.config )
                    {
                        for( var section in res.data.config[app] )
                        {
                            for( var option in res.data.config[app][section] )
                            {
                                //filter comments
                                if( option.indexOf('#')!=0 )
                                {
                                    configs.push({
                                        name : app+'/'+section+'/'+option,
                                        app: app,
                                        section: section,
                                        option: option,
                                        oldValue: res.data.config[app][section][option],
                                        value: ko.observable(res.data.config[app][section][option])
                                    });
                                }
                            }
                        }
                    }
                    self.configTree(configs);
                }
            });
    };
    self.getConfigTree();

    //save specified config param
    self.setConfig = function(param)
    {
        var content = {};
        content.uuid = self.agocontrol.agoController;
        content.command = 'setconfig';
        content.app = param.app;
        content.section = param.section;
        content.option = param.option;
        content.value = param.value(); //observable
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //success
                notif.success('Parameter value updated');
            })
            .catch(function(err) {
                param.value(param.oldValue);
            });
    };
}

/**
 * Initalizes the System model
 */
function init_template(path, params, agocontrol)
{
    var model = new SystemConfig(agocontrol);
    return model;
}

function reset_template(model)
{
    if( model )
    {
        window.clearInterval(model.updateInterval);
    }
}

