/**
 * Model class
 */
function Inventory(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.inventory = ko.observable(JSON.stringify(self.agocontrol.inventory, undefined, 2));

}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new Inventory(agocontrol);
    return model;
}

