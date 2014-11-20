/**
 * Model class
 * 
 * @returns {roomConfig}
 */
function RoomConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.roomName = ko.observable('');

    //after model render
    self.afterRender = function()
    {
        self.agocontrol.stopDatatableLinksPropagation('configTable');
    };

    self.makeEditable = function(row, item)
    {
        window.setTimeout(function()
        {
            $(row).find('td.edit_room').editable(function(value, settings) {
                var content = {};
                content.room = item.uuid;
                content.uuid = self.agocontrol.agoController;
                content.command = "setroomname";
                content.name = value;
                self.agocontrol.sendCommand(content);
                return value;
            }, 
            {
                data : function(value, settings) {
                    return value;
                },
                onblur : "cancel"
            });
        }, 1);
    };

    self.createRoom = function(data, event)
    {
        if( $.trim(self.roomName())!='' )
        {
            self.agocontrol.block($('#configTable'));
            var content = {};
            content.name = self.roomName();
            content.command = 'setroomname';
            content.uuid = self.agocontrol.agoController;
            self.agocontrol.sendCommand(content, function(res) {
                self.roomName('');
                if (res.result && res.result.returncode == 0)
                {
                    self.agocontrol.rooms.push({
                        uuid : res.result.uuid,
                        name : content.name,
                        location : "",
                        action : ""
                    });
                }
                else
                {
                    notif.error("Error while creating room!");
                }
                self.agocontrol.unblock($('#configTable'));
            });
        }
    };

    self.deleteRoom = function(item, event)
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
            self.doDeleteRoom(item, event);
            $("#confirmDelete").dialog("close");
        };
        $("#confirmDelete").dialog({
            modal : true,
            height : 180,
            width : 500,
            buttons : buttons
        });
    };

    self.doDeleteRoom = function(item, event)
    {
        self.agocontrol.block($('#configTable'));
        var content = {};
        content.room = item.uuid;
        content.uuid = self.agocontrol.agoController;
        content.command = 'deleteroom';
        self.agocontrol.sendCommand(content, function(res) {
            if (res.result && res.result.returncode == 0)
            {
                self.agocontrol.rooms.remove(function(e) {
                    return e.uuid == item.uuid;
                });
            }
            else
            {
                notif.error("Error while deleting room!");
            }
            self.agocontrol.unblock($('#configTable'));
        });
    };
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new RoomConfig(agocontrol);
    return model;
}

