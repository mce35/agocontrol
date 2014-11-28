/**
 * Model class
 * 
 * @returns {DeviceConfig}
 */
function DeviceConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.roomFilters = ko.observableArray([]);
    self.deviceTypeFilters = ko.observableArray([]);

    self.updateRoomFilters = function()
    {
        var tagMap = {};
        for ( var i = 0; i < self.agocontrol.devices().length; i++)
        {
            var dev = self.agocontrol.devices()[i];
            if (dev.room)
            {
                if (tagMap["room_" + dev.room])
                {
                    tagMap["room_" + dev.room].w++;
                }
                else
                {
                    tagMap["room_" + dev.room] = {
                        column : "room",
                        value : dev.room,
                        w : 1,
                        selected : false,
                        className : "default label"
                    };
                }
            }
        }

        var roomList = [];
        for ( var k in tagMap)
        {
            var entry = tagMap[k];
            if (entry.column != "devicetype")
            {
                roomList.push(entry);
            }
        }

        roomList.sort(function(a, b) {
            return b.w - a.w;
        });

        self.roomFilters(roomList);
    };

    self.updateDeviceTypeFilters = function()
    {
        var tagMap = {};
        for ( var i = 0; i < self.agocontrol.devices().length; i++)
        {
            var dev = self.agocontrol.devices()[i];
            if (tagMap["type_" + dev.devicetype])
            {
                tagMap["type_" + dev.devicetype].w++;
            }
            else
            {
                tagMap["type_" + dev.devicetype] = {
                    column : "devicetype",
                    value : dev.devicetype,
                    w : 1,
                    selected : false,
                    className : "default label"
                };
            }
        }

        var devList = [];
        for ( var k in tagMap)
        {
            var entry = tagMap[k];
            if (entry.column == "devicetype")
            {
                devList.push(entry);
            }
        }

        devList.sort(function(a, b) {
            return b.w - a.w;
        });

        self.deviceTypeFilters(devList);
    };

    self.findDevice = function(uuid)
    {
        var l = self.agocontrol.devices().filter(function(d) {
            return d.uuid==uuid;
        });
        if(l.length == 1)
            return l[0];
        return null;
    };

    self.findRoom = function(uuid)
    {
        var l = self.agocontrol.rooms().filter(function(d) {
            return d.uuid==uuid;
        });
        if(l.length == 1)
            return l[0];
        return null;
    };

    self.addFilter = function(item)
    {
        var tmp = "";
        var i = 0;
        if (item.column == "devicetype")
        {
            //update selected devicetype filters
            for ( i = 0; i < self.deviceTypeFilters().length; i++)
            {
                if (self.deviceTypeFilters()[i].value == item.value)
                {
                    if( item.className == "default label" )
                    {
                        self.deviceTypeFilters()[i].selected = true;
                        self.deviceTypeFilters()[i].className = "primary label";
                    }
                    else
                    {
                        self.deviceTypeFilters()[i].selected = false;
                        self.deviceTypeFilters()[i].className = "default label";
                    }
                    //self.deviceTypeFilters()[i].className = item.className == "default label" ? "primary label" : "default label";
                }
            }
            tmp = self.deviceTypeFilters();
            self.deviceTypeFilters([]);
            self.deviceTypeFilters(tmp);
        }
        else
        {
            //update selected room filters
            for ( i = 0; i < self.roomFilters().length; i++)
            {
                if (self.roomFilters()[i].value == item.value)
                {
                    if( item.className == "default label" )
                    {
                        item.selected = true;
                        self.roomFilters()[i].className = "primary label";
                    }
                    else
                    {
                        item.selected = false;
                        self.roomFilters()[i].className = "default label";
                    }
                    //self.roomFilters()[i].className = item.className == "default label" ? "primary label" : "default label";
                }
            }
            tmp = self.roomFilters();
            self.roomFilters([]);
            self.roomFilters(tmp);
        }

        //apply filters to grid
        self.grid.resetFilters();
        for( var i=0; i<self.roomFilters().length; i++ )
        {
            if( self.roomFilters()[i].selected )
            {
                self.grid.addFilter('room', self.roomFilters()[i].value);
            }
        }
        for( var i=0; i<self.deviceTypeFilters().length; i++ )
        {
            if( self.deviceTypeFilters()[i].selected )
            {
                self.grid.addFilter('devicetype', self.deviceTypeFilters()[i].value);
            }
        }
    };

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_device') )
        {
            $(td).editable(function(value, settings) {
                var content = {};
                content.device = item.uuid;
                content.uuid = self.agocontrol.agoController;
                content.command = "setdevicename";
                content.name = value;
                self.agocontrol.sendCommand(content, function(res) {
                    if( res!==undefined && res.result!==undefined && res.result!=='no-reply' && res.result.returncode!=-1 )
                    {
                        var d = self.findDevice(item.uuid);
                        d.name = value;
                    }
                    else
                    {
                        notif.error('Error updating device name');
                    }
                });
                return value;
            },
            {
                data : function(value, settings) { return value; },
                onblur : "cancel"
            }).click();
        }
            
        if( $(td).hasClass('select_device_room') )
        {
            var d = self.findDevice(item.uuid);
            if( d && d.name && d.name.length>0 )
            {
                $(td).editable(function(value, settings) {
                    var content = {};
                    content.device = item.uuid;
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setdeviceroom";
                    value = value == "unset" ? "" : value;
                    content.room = value;
                    self.agocontrol.sendCommand(content, function(res) {
                        if( res!==undefined && res.result!==undefined && res.result!=='no-reply' && res.result.returncode!=-1 )
                        {
                            //update inventory
                            var d = self.findDevice(item.uuid);
                            if(value === "")
                            {
                                d.room = d.roomUID = "";
                            }
                            else
                            {
                                var room = self.findRoom(value);
                                if(room)
                                {
                                    d.room = room.name;
                                }
                                d.roomUID = value;
                            }

                            //update room filters
                            self.updateRoomFilters();
                        }
                        else
                        {
                            notif.error('Error updating room');
                        }
                    });
                    if( value==="" )
                    {
                        return "unset";
                    }
                    else
                    {
                        for( var i=0; i<self.agocontrol.rooms().length; i++ )
                        {
                            if( self.agocontrol.rooms()[i].uuid==value )
                            {
                                return self.agocontrol.rooms()[i].name;
                            }
                        }
                    }
                },
                {
                    data : function(value, settings)
                    {
                        var list = {};
                        list["unset"] = "--";
                        for( var i=0; i<self.agocontrol.rooms().length; i++ )
                        {
                            list[self.agocontrol.rooms()[i].uuid] = self.agocontrol.rooms()[i].name;
                        }
                        return JSON.stringify(list);
                    },
                    type : "select",
                    onblur : "submit"
                }).click();
            }
            else
            {
                notif.warning('Please specify device name first');
            }
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.agocontrol.devices,
        columns: [
            {headerText:'Name', rowText:'name'},
            {headerText:'Room', rowText:'room'},
            {headerText:'Device type', rowText:'devicetype'},
            {headerText:'Handled by', rowText:'handledBy'},
            {headerText:'Internalid', rowText:'internalid'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate'
    });

    self.deleteDevice = function(item, event)
    {
        var button_yes = $("#confirmDeleteButtons").data("yes");
        var button_no = $("#confirmDeleteButtons").data("no");
        var buttons = {};
        buttons[button_no] = function() {
            $("#confirmDelete").dialog("close");
        };
        buttons[button_yes] = function() {
            self.doDeleteDevice(item, event);
            $("#confirmDelete").dialog("close");
        };
        $("#confirmDelete").dialog({
            modal : true,
            height : 180,
            width : 500,
            buttons : buttons
        });
    };

    self.doDeleteDevice = function(item, event)
    {
        self.agocontrol.block($('#configTable'));
        var request = {};
        request.method = "message";
        request.params = {};
        request.params.content = {
            uuid : item.uuid
        };
        request.params.subject = "event.device.remove";
        request.id = 1;
        request.jsonrpc = "2.0";

        $.ajax({
            type : 'POST',
            url : self.agocontrol.url,
            data : JSON.stringify(request),
            success : function() {
                var content = {};
                content.device = item.uuid;
                content.uuid = self.agocontrol.agoController;
                content.command = "setdevicename";
                content.name = "";
                self.agocontrol.sendCommand(content, function() {
                    self.agocontrol.devices.remove(function(e) {
                        return e.uuid == item.uuid;
                    });
                    self.agocontrol.unblock($('#configTable'));
                });
            },
            dataType : "json",
            async : true
        });
    };

    //update filters
    self.updateRoomFilters();
    self.updateDeviceTypeFilters();
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new DeviceConfig(agocontrol);
    return model;
}

