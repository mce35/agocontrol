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

    self.afterRender = function()
    {
        self.agocontrol.stopDatatableLinksPropagation('configTable');
    };

    self.devices = ko.computed(function() {
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
                    type : "device",
                    value : dev.devicetype,
                    w : 1,
                    className : "default label"
                };
            }
            if (dev.room)
            {
                if (tagMap["room_" + dev.room])
                {
                tagMap["room_" + dev.room].w++;
                }
                else
                {
                    tagMap["room_" + dev.room] = {
                        type : "room",
                        value : dev.room,
                        w : 1,
                        className : "default label"
                    };
                }
            }
        }

        var devList = [];
        var roomList = [];

        for ( var k in tagMap)
        {
            var entry = tagMap[k];
            if (entry.type == "device")
            {
                devList.push(entry);
            }
            else
            {
                roomList.push(entry);
            }
        }

        devList.sort(function(a, b) {
            return b.w - a.w;
        });

        roomList.sort(function(a, b) {
            return b.w - a.w;
        });

        self.roomFilters(roomList);
        self.deviceTypeFilters(devList);
    });

    this.findDevice = function(uuid)
    {
        var l = self.agocontrol.devices().filter(function(d) {
            return d.uuid==uuid;
        });
        if(l.length == 1)
            return l[0];
        return null;
    };

    this.findRoom = function(uuid)
    {
        var l = self.agocontrol.rooms().filter(function(d) {
            return d.uuid==uuid;
        });
        if(l.length == 1)
            return l[0];
        return null;
    };

    this.addFilter = function(item)
    {
        if (item.type == "device")
        {
            for ( var i = 0; i < self.deviceTypeFilters().length; i++)
            {
                if (self.deviceTypeFilters()[i].value == item.value)
                {
                    self.deviceTypeFilters()[i].className = item.className == "default label" ? "primary label" : "default label";
                }
            }
            var tmp = self.deviceTypeFilters();
            self.deviceTypeFilters([]);
            self.deviceTypeFilters(tmp);
        }
        else
        {
            for ( var i = 0; i < self.roomFilters().length; i++)
            {
                if (self.roomFilters()[i].value == item.value)
                {
                    self.roomFilters()[i].className = item.className == "default label" ? "primary label" : "default label";
                }
            }
            var tmp = self.roomFilters();
            self.roomFilters([]);
            self.roomFilters(tmp);
        }

        var eTable = $("#configTable").dataTable();
        self.resetFilter();

        var escapeRegExp = function(str)
        {
            return str.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, "\\$&");
        };

        var filters = [];
        for ( var i = 0; i < self.deviceTypeFilters().length; i++)
        {
            var tmp = self.deviceTypeFilters()[i];
            if (tmp.className == "primary label")
            {
                filters.push(escapeRegExp(tmp.value));
            }
        }
        if (filters.length > 0)
        {
            eTable.fnFilter("^(" + filters.join("|") + ")$", 2, true, false);
        }

        var filters = [];
        for ( var i = 0; i < self.roomFilters().length; i++)
        {
            var tmp = self.roomFilters()[i];
            if (tmp.className == "primary label")
            {
                filters.push(escapeRegExp(tmp.value));
            }
        }

        if (filters.length > 0)
        {
            eTable.fnFilter("^(" + filters.join("|") + ")$", 1, true, false);
        }
    };

    this.resetFilter = function()
    {
        var eTable = $("#configTable").dataTable();
        var oSettings = eTable.fnSettings();
        for ( var i = 0; i < oSettings.aoPreSearchCols.length; i++)
        {
            oSettings.aoPreSearchCols[i].sSearch = "";
        }
        oSettings.oPreviousSearch.sSearch = "";
        eTable.fnDraw();
    };

    this.makeEditable = function(row, item)
    {
        window.setTimeout(function() {
            $(row).find('td.edit_device').editable(function(value, settings) {
                var content = {};
                content.device = item.uuid;
                content.uuid = self.agocontrol.agoController;
                content.command = "setdevicename";
                content.name = value;
                self.agocontrol.sendCommand(content, function(res) {
                    var d = self.findDevice(item.uuid);
                    d.name = value;
                });
                return value;
            },
            {
                data : function(value, settings)
                {
                    return value;
                },
                onblur : "cancel"
            });

            $(row).find('td.select_device_room').editable(function(value, settings) {
                var content = {};
                content.device = item.uuid;
                content.uuid = self.agocontrol.agoController;
                content.command = "setdeviceroom";
                value = value == "unset" ? "" : value;
                content.room = value;
                self.agocontrol.sendCommand(content, function(res) {
                    var d = self.findDevice(item.uuid);
                    if(value == "")
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
                });
                return value == "" ? "unset" : self.agocontrol.rooms()[value].name;
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
            });
        }, 1);
    };

    this.deleteDevice = function(item, event)
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

    this.doDeleteDevice = function(item, event)
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
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new DeviceConfig(agocontrol);
    return model;
}
