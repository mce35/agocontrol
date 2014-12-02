/**
 * Model class
 * 
 * @returns {VariablesConfig}
 */
function VariablesConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_var') )
        {
            $(td).editable(
                function(value, settings)
                {
                    var content = {};
                    content.variable = $(this).data('variable');
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setvariable";
                    content.value = value;
                    self.agocontrol.sendCommand(content);
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
            {headerText:'Name', rowText:'name'},
            {headerText:'Value', rowText:'value'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate'
    });

    self.createVariable = function(data, event)
    {
        self.agocontrol.block($('#agoGrid'));
        var content = {};
        content.variable = $("#varName").val();
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
        var button_yes = $("#confirmDeleteButtons").data("yes");
        var button_no = $("#confirmDeleteButtons").data("no");
        var buttons = {};
        buttons[button_no] = function()
        {
            $("#confirmDelete").dialog("close");
        };
        buttons[button_yes] = function()
        {
            self.doDeleteVariable(item, event);
            $("#confirmDelete").dialog("close");
        };
        $("#confirmDelete").dialog({
            modal : true,
            height : 180,
            width : 500,
            buttons : buttons
        });
    };

    self.doDeleteVariable = function(item, event)
    {
        self.agocontrol.block($('#agoGrid'));
        var content = {};
        content.variable = item.variable;
        content.uuid = self.agocontrol.agoController;
        content.command = 'delvariable';
        self.agocontrol.sendCommand(content, function(res) {
            if (res.result && res.result.returncode == 0)
            {
                self.agocontrol.variables.remove(function(e) {
                    return e.variable == item.variable;
                });
            }
            else
            {
                notif.error("Error while deleting variable!");
            }
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
