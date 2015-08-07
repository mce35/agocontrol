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
    self.drainAutoscroll = ko.observable(true);

    //append received event
    self.eventHandler = function(event, type)
    {
        if( event && event.event!='event.device.announce')
        {
            //get current date
            var d = formatDate(new Date());

            //append new message
            if( type===undefined )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="default alert">'+d+' : '+JSON.stringify(event)+'</i>');
            }
            else if( type=='start' || type=='stop' )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="primary alert">'+d+' : '+JSON.stringify(event)+'</i>');
            }

            //scroll down
            if( self.drainAutoscroll() )
            {
                $('#drainContainer > ul').scrollTop($('#drainContainer > ul')[0].scrollHeight);
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

