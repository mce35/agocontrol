/**
 * Model class
 */
function AgocontrolVersion(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new AgocontrolVersion(agocontrol);
    return model;
}

