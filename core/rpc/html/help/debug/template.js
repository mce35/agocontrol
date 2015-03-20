/**
 * Model class
 */
function Debug(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;

    //===============
    //DRAIN
    //===============
    self.draining = false;

    //append received event
    self.eventHandler = function(event, type)
    {
        if( event )
        {
            //get current date
            var d = formatDate(new Date());
            if( type===undefined )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="default alert">'+d+' : '+JSON.stringify(event)+'</i>');
            }
            else if( type=='start' || type=='stop' )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="primary alert">'+d+' : '+JSON.stringify(event)+'</i>');
            }
        }
    };

    //start drain
    self.startDrain = function()
    {
        if( !self.draining )
        {
            //add event handler
            self.agocontrol.addEventHandler(self.eventHandler);
            self.eventHandler('Drain started', 'start');
            self.draining = true;
        }
    };

    //stop drain
    self.stopDrain = function()
    {
        if( self.draining )
        {
            self.agocontrol.removeEventHandler(self.eventHandler);
            self.eventHandler('Drain stopped', 'stop');
            self.draining = false;
        }
    };

    //clear
    self.clear = function()
    {
        $('#drainContainer > ul').empty();
    };

    //==============
    //CONFIGURATION
    //==============
    self.configTree = ko.observableArray([]);
    self.configTreeGrid = new ko.agoGrid.viewModel({
        data: self.configTree,
        columns: [
            {headerText:'Parameter', rowText:'name'},
            {headerText:'Value', rowText:'value'}
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
        self.agocontrol.sendCommand(content, function(res) {
            if( true ) //TODO
            {
                var configs = [];
                for( var module in res.result.config )
                {
                    for( var section in res.result.config[module] )
                    {
                        for( var param in res.result.config[module][section] )
                        {
                            //filter comments
                            if( param.indexOf('#')!=0 )
                            {
                                configs.push({
                                    name : module+'/'+section+'/'+param,
                                    module: module,
                                    section: section,
                                    param: param,
                                    value: res.result.config[module][section][param]
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
};

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new Debug(agocontrol);
    return model;
};

function reset_template(model)
{
    if( model )
    {
        model.stopDrain();
    }
};

