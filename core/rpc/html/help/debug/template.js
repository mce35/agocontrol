/**
 * Model class
 */
function Debug(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
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

