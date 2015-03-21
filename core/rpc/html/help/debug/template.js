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
        self.agocontrol.sendCommand(content, function(res) {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.config )
                {
                    var configs = [];
                    for( var app in res.result.config )
                    {
                        for( var section in res.result.config[app] )
                        {
                            for( var option in res.result.config[app][section] )
                            {
                                //filter comments
                                if( option.indexOf('#')!=0 )
                                {
                                    configs.push({
                                        name : app+'/'+section+'/'+option,
                                        app: app,
                                        section: section,
                                        option: option,
                                        oldValue: res.result.config[app][section][option],
                                        value: ko.observable(res.result.config[app][section][option])
                                    });
                                }
                            }
                        }
                    }
                    self.configTree(configs);
                }
                else
                {
                    notif.fatal('agoresolver is not responding');
                }
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
        self.agocontrol.sendCommand(content, function(res) {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.returncode===0 )
                {
                    //success
                    notif.success('Parameter value updated');
                }
                else
                {
                    //unable to set param, revert update
                    notif.error('Unable to set parameter');
                    param.value(param.oldValue);
                }
            }
            else
            {
                notif.fatal('agoresolver is not responding');
            }
        });
    };
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

