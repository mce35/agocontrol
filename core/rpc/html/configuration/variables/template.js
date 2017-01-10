/**
 * Model class
 * 
 * @returns {VariablesConfig}
 */
function VariablesConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.newVariable = ko.observable('');

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_var') )
        {
            $(td).editable(
                function(value, settings)
                {
                    var varName = $(this).data('variable');
                    var v = self.agocontrol.variables.find(function(v){return v.variable == varName});
                    if(!v || v.value === value) return;

                    var content = {};
                    content.variable = varName;
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setvariable";
                    content.value = value;
                    self.agocontrol
                        .sendCommand(content)
                        .then(function(r){
                            // refresh local inventory
                            v.value = value;
                        });
                    return value;
                },
                {
                    data : function(value, settings) {
                        return value;
                    },
                    onblur : "cancel"
                }
            ).click();
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.agocontrol.variables,
        columns: [
            {headerText:'Name', rowText:'variable'},
            {headerText:'Value', rowText:'value'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate',
        boxStyle: 'box-primary'
    });

    self.createVariable = function(data, event)
    {
        self.agocontrol.block($('#agoGrid'));
        var content = {};
        content.variable = self.newVariable();
        content.value = "True";
        content.command = 'setvariable';
        content.uuid = self.agocontrol.agoController;
        self.agocontrol.sendCommand(content, function(res) {
            if (res.result && res.result.returncode == 0)
            {
                self.agocontrol.variables.push({
                    variable : content.variable,
                    value : content.value,
                    action : ""
                });
                self.newVariable('');
            }
            else
            {
                notif.error("Error while creating variable!");
            }
            self.agocontrol.unblock($('#agoGrid'));
        });
    };

    self.deleteVariable = function(item, event)
    {
        $("#confirmPopup").data('item', item);
        $("#confirmPopup").modal('show');
    };

    self.doDeleteVariable = function()
    {
        self.agocontrol.block($('#agoGrid'));
        $("#confirmPopup").modal('hide');

        var item = $("#confirmPopup").data('item');
        var content = {};
        content.variable = item.variable;
        content.uuid = self.agocontrol.agoController;
        content.command = 'delvariable';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.agocontrol.variables.remove(function(e) {
                    return e.variable == item.variable;
                });
            })
            .catch(function(err) {
                notif.error("Error while deleting variable!");
            })
            .finally(function() {
                self.agocontrol.unblock($('#agoGrid'));
            });
    };
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new VariablesConfig(agocontrol);
    return model;
}
