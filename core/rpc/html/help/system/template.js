/**
 * Model class
 * 
 * @returns {systemConfig}
 */
function SystemConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
}

/**
 * Initalizes the System model
 */
function init_template(path, params, agocontrol)
{
    var model = new SystemConfig(agocontrol);
    return model;
}
