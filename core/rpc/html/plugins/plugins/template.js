/**
 * List of available plugins
 */
function Plugins(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    
    //filter dashboard that don't need to be displayed
    //need to do that because datatable odd is broken when filtering items using knockout
    self.plugins = ko.computed(function() {
        var plugins = [];
        for( var i=0; i<self.agocontrol.plugins().length; i++ )
        {
            if( self.agocontrol.plugins()[i].listable )
            {
                plugins.push(self.agocontrol.plugins()[i]);
            }
        }
        return plugins;
    });
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new Plugins(agocontrol);
    return model;
}

