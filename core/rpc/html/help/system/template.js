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
        self.agocontrol.sendCommand(content, function(res) {
            var procs = [];
            for( var name in res.result )
            {
                var proc = {};
                proc.name = name;
                proc.running = res.result[name].running;
                proc.cpu = (Math.round((res.result[name].currentStats.ucpu + res.result[name].currentStats.scpu)*10)/10)+'%';
                proc.memVirt = res.result[name].currentStats.vsize;
                proc.memRes = res.result[name].currentStats.rss;
                proc.monitored = res.result[name].monitored;
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
 * Initalizes the System model
 */
function init_template(path, params, agocontrol)
{
    ko.bindingHandlers.initTabs = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            $('.tab-nav li > a').click(function() {
                $(".tab-nav li").each(function() {
                    $(this).removeClass('active');
                });

                $(this).parent().toggleClass('active');

                this.$el = $('.tabs');
                var index = $(this).parent().index();
                this.$content = this.$el.find(".tab-content");
                this.$nav = $(this).parent().find('li');
                this.$nav.add(this.$content).removeClass("active");
                this.$nav.eq(index).add(this.$content.eq(index)).addClass("active");
                return false; //prevent from bubbling
            });
        },
    };

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

