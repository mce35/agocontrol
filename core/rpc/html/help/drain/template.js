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
        if( event )
        {
            //get current date
            var d = formatDate(new Date());

            //append new message
            if( type===undefined )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="list-group-item">'+d+' : '+JSON.stringify(event)+'</i>');
            }
            else if( type=='start' || type=='stop' )
            {
                $('#drainContainer > ul').append('<li style="font-size:small;" class="list-group-item" list-group-item-info>'+d+' : '+JSON.stringify(event)+'</i>');
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

