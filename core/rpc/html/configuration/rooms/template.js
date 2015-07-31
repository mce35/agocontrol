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

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_room') )
        {
            $(td).editable(
                function(value, settings)
                {
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
                }
            ).click();
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.agocontrol.rooms,
        columns: [
            {headerText:'Name', rowText:'name'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate',
        boxStyle: 'box-primary'
    });

    self.createRoom = function(data, event)
    {
        if( $.trim(self.roomName())!='' )
        {
            self.agocontrol.block($('#agoGrid'));
            var content = {};
            content.name = self.roomName();
            content.command = 'setroomname';
            content.uuid = self.agocontrol.agoController;
            self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.roomName('');
                self.agocontrol.rooms.push({
                    uuid : res.data.uuid,
                    name : content.name,
                    location : "",
                    action : ""
                });
            })
            .catch(function(err) {
                console.error(err);
            })
            .finally(function() {
                self.agocontrol.unblock($('#agoGrid'));
            });
        }
    };

    self.deleteRoom = function(item, event)
    {
        $("#confirmPopup").data('item', item);
        $("#confirmPopup").modal('show');
    };

    self.doDeleteRoom = function()
    {
        self.agocontrol.block($('#agoGrid'));
        $("#confirmPopup").modal('hide');

        var item = $("#confirmPopup").data('item');
        var content = {};
        content.room = item.uuid;
        content.uuid = self.agocontrol.agoController;
        content.command = 'deleteroom';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.agocontrol.rooms.remove(function(e) {
                    return e.uuid == item.uuid;
                });
            })
            .catch(function(err) {
                notif.error("Error while deleting room!");
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
    var model = new RoomConfig(agocontrol);
    return model;
}

