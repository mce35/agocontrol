/**
 * Model class
 * 
 * @returns {ScenarioConfig}
 */
function ScenarioConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.scenarioName = ko.observable('');
    this.scenarios = ko.computed(function() {
        return self.agocontrol.devices().filter(function(d) {
            return d.devicetype=='scenario';
        });
    });

    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('edit_scenario') )
        {
            $(td).editable(
                function(value, settings)
                {
                    var content = {};
                    content.device = item.uuid;
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setdevicename";
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
                }
            ).click();
        }
        else if( $(td).hasClass('select_room') )
        {
            $(td).editable(
                function(value, settings)
                {
                    var content = {};
                    content.device = $(this).data('uuid');
                    content.uuid = self.agocontrol.agoController;
                    content.command = "setdeviceroom";
                    content.room = value == "unset" ? "" : value;
                    self.agocontrol.sendCommand(content);
                    var name = "unset";
                    for( var i=0; i<self.agocontrol.rooms().length; i++ )
                    {
                        if( self.agocontrol.rooms()[i].uuid==value )
                        {
                            name = self.agocontrol.rooms()[i].name;
                        }
                    }
                    return value == "unset" ? "unset" : name;
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
                }
            ).click();
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.scenarios,
        columns: [
            {headerText:'Name', rowText:'name'},
            {headerText:'Room', rowText:'room'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.makeEditable,
        rowTemplate: 'rowTemplate',
        boxStyle: 'box-primary'
    });

    //Creates a scenario map out of the form fields inside a container
    self.buildScenarioMap = function(containerID)
    {
        var map = {};
        var map_idx = 0;
        var commands = document.getElementById(containerID).childNodes;
        for ( var i = 0; i < commands.length; i++)
        {
            var command = commands[i];
            var tmp = {};
            for ( var j = 0; j < command.childNodes.length; j++)
            {
                var child = command.childNodes[j];
                if (child.name && child.name == "device" && child.options[child.selectedIndex].value != "sleep")
                {
                    tmp.uuid = child.options[child.selectedIndex].value;
                }
                else if (child.tagName == "DIV")
                {
                    for ( var k = 0; k < child.childNodes.length; k++)
                    {
                        var subChild = child.childNodes[k];
                        if (subChild.name && subChild.name == "command")
                        {
                            tmp.command = subChild.options[subChild.selectedIndex].value;
                        }
                        if (subChild.name && subChild.type && subChild.type == "text")
                        {
                            tmp[subChild.name] = subChild.value;
                        }
                    }
                }
            }
            map[map_idx++] = tmp;
        }

        return map;
    };

    //Sends the create scenario command
    self.createScenario = function()
    {
        if( $.trim(self.scenarioName())=='' )
        {
            notif.warning("Please type a scenario name!");
            return;
        }

        self.agocontrol.block($('#agoGrid'));

        var content = {};
        content.command = "setscenario";
        content.uuid = self.agocontrol.scenarioController;
        content.scenariomap = self.buildScenarioMap("scenarioBuilder");
        self.agocontrol.sendCommand(content)
        .then(function(res) {
            var cnt = {};
            cnt.uuid = self.agocontrol.agoController;
            cnt.device = res.data.scenario;
            cnt.command = "setdevicename";
            cnt.name = self.scenarioName();
            return self.agocontrol.sendCommand(cnt)
            .then(function(nameRes) {
                self.agocontrol.refreshDevices(false);
                self.scenarioName('');
                $('#scenarioBuilder').html("");
            });
        })
        .catch(function(err) {
            notif.warning("Please add commands before creating the scenario!");
        })
        .finally(function() {
            self.agocontrol.unblock($('#agoGrid'));
        });
    };

    //Adds a command selection entry
    self.addCommand = function(containerID, defaultValues)
    {
        if (!containerID)
        {
            containerID = "scenarioBuilder";
        }

        var row = document.createElement("div");

        //delete button
        var removeBtn = document.createElement("button");
        removeBtn.setAttribute("style", "display:inline");
        removeBtn.setAttribute("class", "btn btn-danger btn-sm");
        removeBtn.setAttribute("type", "button");
        removeBtn.innerHTML = '<span class="en-cancel"></span>';
        row.appendChild(removeBtn);
        removeBtn.onclick = function()
        {
            row.parentNode.removeChild(row);
        };

        // Move up button
        var upBtn = document.createElement("button");
        upBtn.style.display = "inline";
        upBtn.setAttribute("type", "button");
        upBtn.setAttribute("class", "btn btn-default btn-sm");
        upBtn.innerHTML = '<span class="en-up"></span>';
        upBtn.onclick = function()
        {
            var prev = row.previousSibling;
            document.getElementById(containerID).removeChild(row);
            document.getElementById(containerID).insertBefore(row, prev);
        };
        row.appendChild(upBtn);

        // Move down button
        var downBtn = document.createElement("button");
        downBtn.style.display = "inline";
        downBtn.setAttribute("type", "button");
        downBtn.setAttribute("class", "btn btn-default btn-sm");
        downBtn.innerHTML = '<span class="en-down"></span>';
        downBtn.onclick = function()
        {
            var next = row.nextSibling;
            document.getElementById(containerID).removeChild(next);
            document.getElementById(containerID).insertBefore(next, row);
        };
        row.appendChild(downBtn);

        //device list
        var deviceSelect = document.createElement("select");
        deviceSelect.setAttribute("name", "device");
        deviceSelect.setAttribute("class", "form-control input-sm");
        deviceSelect.setAttribute("style", "display:inline; width:200px");
        deviceSelect.options.length = 0;
        self.agocontrol.devices().sort(function(a, b) {
            return a.room.localeCompare(b.room);
        });
        for ( var i = 0; i < self.agocontrol.devices().length; i++)
        {
            var dev = self.agocontrol.devices()[i];
            if( self.agocontrol.schema().devicetypes[dev.devicetype] && self.agocontrol.schema().devicetypes[dev.devicetype].commands.length > 0 && dev.name() )
            {
                var dspName = "";
                if (dev.room)
                {
                    dspName = dev.name()+' ('+dev.room+')';
                }
                else
                {
                    dspName = dev.name();
                }
                deviceSelect.options[deviceSelect.options.length] = new Option(dspName, dev.uuid);
                deviceSelect.options[deviceSelect.options.length - 1]._dev = dev;
                if (defaultValues && defaultValues.uuid == dev.uuid)
                {
                    deviceSelect.selectedIndex = deviceSelect.options.length - 1;
                }
            }
        }

        // Special case for the sleep command
        deviceSelect.options[deviceSelect.options.length] = new Option("Sleep", "sleep");
        deviceSelect.options[deviceSelect.options.length - 1]._dev = "sleep";
        if (defaultValues && !defaultValues.uuid)
        {
            deviceSelect.selectedIndex = deviceSelect.options.length - 1;
        }

        row.appendChild(deviceSelect);

        //dynamic container for selected parameters
        var commandContainer = document.createElement("div");
        commandContainer.style.display = "inline";

        deviceSelect.onchange = function()
        {
            commandContainer.innerHTML = "";
            var dev = deviceSelect.options[deviceSelect.selectedIndex]._dev;
            var commands = document.createElement("select");
            commands.setAttribute("name", "command");
            commands.setAttribute("class", "form-control input-sm");
            commands.setAttribute("style", "display:inline; width:200px");
            if (dev != "sleep")
            {
                for ( var i = 0; i < self.agocontrol.schema().devicetypes[dev.devicetype].commands.length; i++)
                {
                    var cmd = self.agocontrol.schema().devicetypes[dev.devicetype].commands[i];
                    commands.options[i] = new Option(self.agocontrol.schema().commands[cmd].name, cmd);
                    commands.options[i]._cmd = self.agocontrol.schema().commands[cmd];
                    if (defaultValues && defaultValues.command == cmd)
                    {
                        commands.selectedIndex = i;
                    }
                }
            }
            else
            {
                // Special case for the sleep command
                commands.options[commands.options.length] = new Option("Delay", "scenariosleep");
                commands.options[commands.options.length - 1]._cmd = "sleep";
                if (defaultValues && defaultValues.command == "scenariosleep")
                {
                    commands.selectedIndex = commands.options.length - 1;
                }
            }
            commands.style.display = "inline";
            commandContainer.appendChild(commands);
            commands.onchange = function()
            {
                if (commandContainer._params)
                {
                    for ( var i = 0; i < commandContainer._params.length; i++)
                    {
                        try
                        {
                            commandContainer.removeChild(commandContainer._params[i]);
                        }
                        catch (e)
                        {
                            // ignore node is gone
                        }
                    }
                    commandContainer._params = null;
                }

                var cmd = commands.options[commands.selectedIndex]._cmd;
                if (cmd.parameters)
                {
                    commandContainer._params = [];
                    for ( var key in cmd.parameters)
                    {
                        var field = document.createElement("input");
                        field.setAttribute("type", "text");
                        field.setAttribute("name", key);
                        field.setAttribute("class", "form-control input-sm");
                        field.setAttribute("style", "display:inline; width:150px;");
                        field.setAttribute("placeholder", cmd.parameters[key].name);
                        if (defaultValues && defaultValues[key])
                        {
                            field.setAttribute("value", defaultValues[key]);
                        }
                        commandContainer._params.push(field);
                        commandContainer.appendChild(field);
                    }
                }
                else if (cmd == "sleep")
                {
                    // Special case for the sleep command
                    commandContainer._params = [];
                    var field = document.createElement("input");
                    field.setAttribute("type", "text");
                    field.setAttribute("name", "delay");
                    field.setAttribute("class", "form-control input-sm");
                    field.setAttribute("style", "display:inline; width:150px;");
                    field.setAttribute("placeholder", "Delay in seconds");
                    if (defaultValues && defaultValues["delay"])
                    {
                        field.setAttribute("value", defaultValues.delay);
                    }
                    commandContainer._params.push(field);
                    commandContainer.appendChild(field);
                }
            };

            if (commands.options.length > 0)
            {
                commands.onchange();
            }
        };
        deviceSelect.onchange();
        row.appendChild(commandContainer);

        document.getElementById(containerID).appendChild(row);
    };

    self.deleteScenario = function(item, event)
    {
        $("#confirmPopup").data('item', item);
        $("#confirmPopup").modal('show');
    };

    //Sends the delete scenario command
    self.doDeleteScenario = function()
    {
        self.agocontrol.block($('#agoGrid'));
        $("#confirmPopup").modal('hide');

        var item = $("#confirmPopup").data('item');
        var content = {};
        content.scenario = item.uuid;
        content.uuid = self.agocontrol.scenarioController;
        content.command = 'delscenario';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.agocontrol.devices.remove(function(e) {
                    return e.uuid == item.uuid;
                });
            })
            .catch(function(err) {
                notif.error("Error while deleting scenarios!");
            })
            .finally(function() {
                self.agocontrol.unblock($('#agoGrid'));
            });
    };

    self.editScenario = function(item)
    {
        var content = {};
        content.scenario = item.uuid;
        content.uuid = self.agocontrol.scenarioController;
        content.command = 'getscenario';
        self.agocontrol.sendCommand(content)
        .then(function(res) {
            // Build command list
            $('#scenarioBuilderEdit').html('');
            for ( var idx in res.data.scenariomap)
            {
                self.addCommand("scenarioBuilderEdit", res.data.scenariomap[idx]);
            }

            // Save the id (needed for the save command)
            self.openScenario = item.uuid;

            // Open the dialog
            $("#editPopup").modal('show');
        });
    };

    self.doEditScenario = function()
    {
        var content = {};
        content.command = "setscenario";
        content.uuid = self.agocontrol.scenarioController;
        content.scenario = self.openScenario;
        content.scenariomap = self.buildScenarioMap("scenarioBuilderEdit");
        self.agocontrol.sendCommand(content)
        .then(function(res) {
            $('#scenarioBuilderEdit').html('');
            self.openScenario = null;
            $("#editPopup").modal('hide');
        });
    };

    self.runScenario = function(item)
    {
        var content = {};
        content.uuid = item.uuid;
        content.command = 'on';
        self.agocontrol.sendCommand(content);
    };

}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new ScenarioConfig(agocontrol);
    return model;
}

