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
}

/**
 * Initalizes the System model
 */
function init_template(path, params, agocontrol)
{
    var model = new SystemConfig(agocontrol);
    return model;
}
