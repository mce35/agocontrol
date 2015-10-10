/**
 * KNX plugin
 */
 
function KNX(agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.controllerUuid = null;
    self.ga = null;
    self.gaLoaded = ko.observable(false);
    self.treeviewDisplayed = false;
    self.devices = ko.observableArray([]);
    self.selectedNode = null;
    self.deviceTypes = ko.observableArray([]);
    self.selectedDeviceType = ko.observable(null);
    self.deviceTypeParameters = ko.observableArray([]);
    self.selectedDeviceType.subscribe(function() {
        var out = [];
        for( var i=0; i<self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['childdevices'][self.selectedDeviceType()].length; i++ )
        {
            var item = {};
            item['key'] = self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['childdevices'][self.selectedDeviceType()][i];
            item['value'] = self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['groupaddresstypes'][item['key']]['name'];
            item['label'] = ko.observable('None');
            item['id'] = null;
            out.push(item);
        }
        self.deviceTypeParameters(out);
    });
    self.selectedDeviceTypeParam = null;
    self.gaIdValue = {};
    self.devices = ko.observableArray([]);
    self.selectedDevice = ko.observable(undefined);
    self.selectedDeviceTypeEdit = ko.observable(null);
    self.selectedDevice.subscribe(function() {
        if( self.selectedDevice() )
        {
            //console.log('selecteddevice');
            //console.log(self.selectedDevice());
            self.selectedDeviceTypeEdit(self.selectedDevice().devicetype);
        }
        else
        {
            self.selectedDeviceTypeEdit(undefined);
        }
    });
    self.deviceTypeParametersEdit = ko.observableArray([]);
    self.selectedDeviceTypeEdit.subscribe(function() {
        if( self.selectedDeviceTypeEdit() )
        {
            //console.log(JSON.stringify(self.selectedDevice()));
            var params = self.selectedDevice().params;
            //console.log('PARAMS');
            //console.log(params);
            var out = [];
            for( var i=0; i<self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['childdevices'][self.selectedDeviceTypeEdit()].length; i++ )
            {
                var item = {};
                item['key'] = self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['childdevices'][self.selectedDeviceTypeEdit()][i];
                item['value'] = self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['groupaddresstypes'][item['key']]['name'];
                for( var j=0; j<self.selectedDevice().params.length; j++ )
                {
                    var param = self.selectedDevice().params[j];
                    if( param['type']===item['key'] )
                    {
                        //fill current param with existing values
                        item['label'] = ko.observable(param['label']);
                        item['id'] = param['id'];
                        break;
                    }
                    else
                    {
                        //fill with default values
                        item['label'] = ko.observable('None');
                        item['id'] = null;
                    }
                }
                out.push(item);
            }
            self.deviceTypeParametersEdit(out);
        }
        else
        {
            self.selectedDeviceTypeEdit(self.deviceTypes[0]);
            self.deviceTypeParametersEdit([]);
        }
    });
   
    //get controller uuid and fill device types
    self.getControllerUuid = function()
    {
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=='knxcontroller' )
            {
                //console.log(self.agocontrol.devices()[i]);
                self.controllerUuid = self.agocontrol.devices()[i].uuid;
            }
        }

        var deviceTypes = [];
        if( self.agocontrol.schema() )
        {
            try
            {
                //console.log(self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']);
                for( var type in self.agocontrol.schema()['devicetypes']['knxcontroller']['internal']['childdevices'] )
                {
                    deviceTypes.push(type);
                }
            }
            catch(e)
            {
                console.error('Unable to get knxcontroller data from schema [' + e + ']');
            }
        }
        self.deviceTypes(deviceTypes.sort());
    };

    //get device infos from inventory
    self.getDeviceInfos = function(uuid)
    {
        if( self.agocontrol.inventory.devices[uuid]!==undefined )
        {
            return self.agocontrol.inventory.devices[uuid];
        }
        else
        {
            console.error('No infos found in inventory for device ' + uuid);
            return {};
        }
    };

    //prepare file import
    self.prepareImport = function()
    {
        //init upload
        $('#fileupload').fileupload({
            dataType: 'json',
            formData: { 
                uuid: self.controllerUuid
            },
            done: function(e, data) {
                //TODO fill ga directly
                self.getGAContent();
            },
            progressall: function (e, data) {
                var progress = parseInt(data.loaded / data.total * 100, 10);
                $('#progress').css('width', progress+'%');
            }
        });
    }; 

    //build treeview
    self.buildTreeview = function()
    {
        //https://javascriptweblog.wordpress.com/2011/08/08/fixing-the-javascript-typeof-operator/
        var toType = function(obj)
        {
              return ({}).toString.call(obj).match(/\s([a-zA-Z]+)/)[1].toLowerCase()
        };

        var iterateItem = function(item)
        {
            var branch = [];
            for( var el in item )
            {
                //console.log(''+el+' ('+toType(item[el])+')');
                var elem = {};
                if( toType(item[el])=='object' )
                {
                    //branch
                    elem['text'] = el;
                    elem['selectable'] = false;
                    elem['nodes'] = iterateItem(item[el]);
                }
                else
                {
                    //leaf
                    elem['text'] = el + ' (' + item[el] + ')';
                    elem['selectable'] = true;
                    elem['value'] = item[el];
                    self.gaIdValue[item[el]] = el;
                }
                branch.push(elem);
            }
            return branch;
        };

        //clear GA id-value map
        self.gaIdValue = {};

        //create treeview
        //console.log('GA');
        //console.log(self.ga);
        var tree = [];
        for( var el in self.ga )
        {
            //console.log(''+el+' ('+toType(self.ga[el])+')');
            var elem = {};
            if( toType(self.ga[el])=='object' )
            {
                //branch
                elem['text'] = el;
                elem['selectable'] = false;
                elem['nodes'] = iterateItem(self.ga[el]);
            }
            else
            {
                //leaf
                elem['text'] = el + ' (' + item[el] + ')';
                elem['selectable'] = true;
                elem['value'] = item[el];
                self.gaIdValue[item[el]] = el;
            }
            tree.push(elem);
        }
        //console.log('TREE');
        console.log(tree);
        //console.log('GAIDVALUE');
        //console.log(self.gaIdValue);

        $('#tree').treeview({data: tree});
        $('#tree').on('nodeSelected', function(event, data) {
            //console.log(data);
            self.selectedNode = data;
        });
        $('#tree').on('nodeUnselected', function(event, data) {
            self.selectedNode = null;
        });
    };

    //load GA content
    self.getGAContent = function()
    {
        var content = {};
        content.uuid = self.controllerUuid;
        content.command = 'getgacontent';

        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( res.data.groupmap )
                {
                    self.ga = res.data.groupmap;
                    self.buildTreeview();
                }
                else
                {
                    self.ga = null;
                }

                //we have ga content, we can get devices
                self.getDevices();
            })
            .catch(function(err){
                notif.error('Unable to get file content');
                self.ga = null;
            })
            .finally(function() {
                //console.log(self.ga);
                if( self.ga===null )
                {
                    self.gaLoaded(false);
                }
                else
                {
                    self.gaLoaded(true);
                }
            });
    };

    //get devices
    self.getDevices = function()
    {
        var content = {};
        content.uuid = self.controllerUuid;
        content.command = 'getdevices';

        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //console.log('DEVICES');
                //console.log(res);
                var devices = [];
                for( var uuid in res.data.devices )
                {
                    var device = {};
                    var infos = self.getDeviceInfos(uuid);
                    device['uuid'] = uuid;
                    device['name'] = infos['name'];
                    device['room'] = infos['room'];
                    device['params'] = [];
                    for( var item in res.data.devices[uuid] )
                    {
                        if( item!=='devicetype' )
                        {
                            //item is parameter, get param label
                            var param = {};
                            param['type'] = item;
                            param['id'] = res.data.devices[uuid][item];
                            param['label'] = self.gaIdValue[res.data.devices[uuid][item]] + ' (' + param['id'] + ')';
                            device['params'].push(param);
                        }
                        else
                        {
                            device[item] = res.data.devices[uuid][item];
                        }
                    }
                    devices.push(device);
                }
                //console.log(devices);
                self.devices(devices);

                //force selectedDevice to first device
                /*if( self.devices().length>0 )
                {
                    self.selectedDevice( self.devices()[self.devices().length-1] );
                    self.selectedDevice( self.devices()[0]);
                }*/
            });
    };

    //add new device
    self.addDevice = function()
    {
        var content = {};
        content.uuid = self.controllerUuid;
        content.command = 'adddevice';
        content.devicemap = {};
        content.devicemap.devicetype = self.selectedDeviceType();
        for( var i=0; i<self.deviceTypeParameters().length; i++ )
        {
            if( self.deviceTypeParameters()[i]['id']===null )
            {
                //item not filled, stop here
                notif.warning('Please fill all ???');
                return;
            }
            content.devicemap[self.deviceTypeParameters()[i]['key']] = self.deviceTypeParameters()[i]['id'];
        }

        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //reset form
                self.selectedDeviceType(self.deviceTypes()[self.deviceTypes().length-1]);
                self.selectedDeviceType(self.deviceTypes()[0]);

                //refresh devices
                self.getDevices();

                //notify user
                notif.success('Device added succesfully');
            });
    };

    //del device
    self.delDevice = function(item, event)
    {
        $("#confirmPopup").data('item', item);
        $("#confirmPopup").modal('show');
    }

    //execute device deletion
    self.doDelDevice = function()
    {
        self.agocontrol.block($('#agoGrid'));
        $("#confirmPopup").modal('hide');

        var item = $("#confirmPopup").data('item');

        var content = {};
        content.uuid = self.controllerUuid;
        content.command = 'deldevice';
        content.device = item.uuid;

        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //refresh devices
                self.getDevices();
                
                //notify user
                notif.success('Device deleted succesfully');
            })
            .finally(function() {
                self.agocontrol.unblock($('#agoGrid'));
            });
    };

    //save device
    self.saveDevice = function()
    {
        var content = {};
        content.uuid = self.controllerUuid;
        content.command = 'adddevice';
        content.devicemap = {};
        var uuid = self.selectedDevice()['uuid'];
        content.devicemap[uuid] = {};
        content.devicemap[uuid].devicetype = self.selectedDeviceTypeEdit();
        for( var i=0; i<self.deviceTypeParametersEdit().length; i++ )
        {
            if( self.deviceTypeParametersEdit()[i]['id']===null )
            {
                //item not filled, stop here
                notif.warning('Please fill all ???');
                return;
            }
            content.devicemap[uuid][self.deviceTypeParametersEdit()[i]['key']] = self.deviceTypeParametersEdit()[i]['id'];
        }

        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //reset form
                //self.selectedDeviceTypeEdit(undefined);
                //self.selectedDeviceType(self.deviceTypes()[0]);

                //refresh devices
                self.getDevices();

                //notify user
                notif.success('Device updated succesfully');
            });
    };

    //edit device
    self.editDevice = function(dev)
    {
        //select device
        self.selectedDevice(dev);

        //and jump to edit tab choosing selected device
        $('#editdevice-href').click()
    };

    //tab changed
    self.onTabChanged = function(index, name)
    {
        if( index===1 )
        {
            //import GA file tab
            self.prepareImport();
        }
    };

    //open treeview popup
    self.openTreeview = function(param)
    {
        self.selectedDeviceTypeParam = param;

        //prepare treeview
        if( !self.treeviewDisplayed )
        {
            self.buildTreeview();
        }
        $('#tree').treeview('collapseAll', { silent: true });
        if( self.selectedNode!==null )
        {
            $('#tree').treeview('unselectNode', [self.selectedNode, { silent: true }]);
        }

        //show modal
        $('#treeviewModal').modal('show');
        self.treeviewDisplayed = true;
    };

    //select choosen device
    self.selectDevice = function()
    {
        self.selectedDeviceTypeParam['label'](self.selectedNode.text); //observable
        self.selectedDeviceTypeParam['id'] = self.selectedNode.value;

        //hide modal
        $('#treeviewModal').modal('hide');
    }

    self.findDevice = function(uuid) {
        var l = self.agocontrol.devices().filter(function(d) {
            return d.uuid==uuid;
        });
        if(l.length == 1)
            return l[0];
        return null;
    };

    //edit row
    self.makeEditable = function(item, td, tr)
    {
        if( $(td).hasClass('change_name') )
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
            
        if( $(td).hasClass('change_room') )
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

    self.devicesGrid = new ko.agoGrid.viewModel({
        data: self.devices,
        columns: [
            {headerText: 'Name', rowText:'name'},
            {headerText: 'Uuid', rowText:'uuid'},
            {headerText: 'Type', rowText:'devicetype'},
            {headerText: 'Room', rowText:'room'},
            {headerText: 'Associations', rowText:''},
            {headerText: 'Actions', rowText:''}
        ],
        rowTemplate: 'deviceRowTemplate',
        rowCallback: self.makeEditable
    });

    //init
    self.getControllerUuid();
    self.getGAContent();
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new KNX(agocontrol);
    return model;
}

