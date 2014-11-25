/**
 * Model class
 * 
 * @returns {DashboardConfig}
 */
function DashboardConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.newDashboardName = ko.observable('');

    //after model render
    self.afterRender = function()
    {
        self.agocontrol.stopDatatableLinksPropagation('floorPlanTable');
    };

    //filter dashboard that don't need to be displayed
    //need to do that because datatable odd is broken when filtering items using knockout
    self.dashboards = ko.computed(function()
    {
        var dashboards = [];
        for( var i=0; i<self.agocontrol.dashboards().length; i++ )
        {
            if( self.agocontrol.dashboards()[i].editable )
            {
                dashboards.push(self.agocontrol.dashboards()[i]);
            }
        }
        return dashboards;
    });

    self.makeEditable = function(row, item)
    {
        window.setTimeout(function()
        {
            $(row).find('td.edit_fp').editable(
                function(value, settings)
                {
                    var content = {};
                    content.floorplan = item.uuid;
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setfloorplanname";
                    content.name = value;
                    self.agocontrol.sendCommand(content);
                    return value;
                },
                {
                    data : function(value, settings)
                    {
                        return value;
                    },
                    onblur : "cancel"
                });
        }, 1);
    };

    self.deletePlan = function(item, event)
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
            self.doDeletePlan(item, event);
            $("#confirmDelete").dialog("close");
        };
        $("#confirmDelete").dialog({
            modal : true,
            height : 180,
            width : 500,
            buttons : buttons
        });
    };

    self.doDeletePlan = function(item, event)
    {
        self.agocontrol.block($('#floorPlanTable'));
        var content = {};
        content.floorplan = item.uuid;
        content.uuid = self.agocontrol.agoController;
        content.command = 'deletefloorplan';
        self.agocontrol.sendCommand(content, function(res)
        {
            if (res.result && res.result.returncode == 0)
            {
                self.agocontrol.refreshDashboards();
            }
            else
            {
                notif.error("Error while deleting floorplan!");
            }
            self.agocontrol.unblock($('#floorPlanTable'));
        });
    };

    self.createDashboard = function()
    {
        if( $.trim(self.newDashboardName())!='' )
        {
            self.agocontrol.block($('#floorPlanTable'));
            var content = {};
            content.command = "setfloorplanname";
            content.uuid = self.agocontrol.agoController;
            content.name = self.newDashboardName();
            self.agocontrol.sendCommand(content, function(response)
            {
                self.newDashboardName('');
                self.agocontrol.refreshDashboards();
                self.agocontrol.unblock($('#floorPlanTable'));
            });
        }
    };
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    model = new DashboardConfig(agocontrol);
    return model;
}
