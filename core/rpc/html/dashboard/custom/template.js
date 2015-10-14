/**
 * Model class
 * 
 * @returns {Dashboard}
 */


function Dashboard(dashboard, edition, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.currentDashboard = dashboard;
    self.dashboardName = ko.observable(ucFirst(self.currentDashboard.name));
    self.editLabel = ko.observable('Edit');
    self.devices = ko.observableArray([]);
    self.selectedTextFilter = ko.observable('');
    self.selectedMainFilter = ko.observable('all');
    self.selectedSubFilter = ko.observable('');
    self.placedDevices = ko.observable([]);
    self.editorOpened = false;
    self.defaultRow = 4;
    self.defaultCol = 4;
    self.dummy = ko.observable();
    self.openEditor = edition;

    self.colNumber = ko.pureComputed(function() {
        if( typeof(Storage)!=='undefined' )
        {
            out  = localStorage.getItem('dashboardColSize');
            if( out===null )
            {
                localStorage.setItem('dashboardColSize', self.defaultCol);
                out = self.defaultCol;
            }
        }
        else
        {
            out = self.defaultCol;
        }
        return out;
    });

    self.rowNumber = ko.computed(function()
    {
        if( typeof(Storage)!=='undefined' )
        {
            out  = localStorage.getItem('dashboardRowSize');
            if( out===null )
            {
                localStorage.setItem('dashboardRowSize', self.defaultCol);
                out = self.defaultRow;
            }
        }
        else
        {
            out = self.defaultRow;
        }
        return out;
    });

    self.devicesPerPage = ko.computed(function() {
        var out = self.colNumber() * self.rowNumber();
    });

    //====================================================
    //EDITOR

    //disable drag and drop
    self.disableDragAndDrop = function()
    {
        //remove special style
        $('.dashboard-widget').removeClass('dashboard-widget-draggable');

        //destroy draggable and droppable
        $('.placeholder').each(function()
        {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });
        $('.device-list').each(function()
        {
            if( $(this).hasClass('ui-droppable') )
            {
                $(this).droppable('destroy');
            }
        });
        $('.device-list-item').each(function()
        {
            if( $(this).hasClass('ui-draggable') )
            {
                $(this).draggable('destroy');
            }
        });
        $('.dashboard-widget').each(function()
        {
            if( $(this).hasClass('ui-draggable') )
            {
                $(this).draggable('destroy');
            }
        });
    };

    //enable drag and drop
    self.enableDragAndDrop = function()
    {
        //exit if not in edition
        if( !self.editorOpened )
        {
            return;
        }

        //add special style
        $('.dashboard-widget').not('.placeholder').addClass('dashboard-widget-draggable');

        //placeholder droppable
        $('.placeholder').each(function()
        {
            $(this).droppable({
                accept: '.device-list-item, .dashboard-widget',
                activate: function(event, ui) {
                    $(this).find('div.info-box:first-child').addClass('border-t-r-l');
                    $(this).find('div.info-box:last-child').addClass('border-r-b-l');
                },
                deactivate: function(event, ui) {
                    $(this).find('div.info-box:first-child').removeClass('border-t-r-l');
                    $(this).find('div.info-box:last-child').removeClass('border-r-b-l');
                },
                over: function(event, ui) {
                    $(this).find('div.info-box:first-child').removeClass('bg-white').addClass('bg-aqua');
                    $(this).find('div.info-box:last-child').removeClass('bg-white').addClass('bg-aqua');
                },
                out: function(event, ui) {
                    $(this).find('div.info-box:first-child').removeClass('bg-aqua').addClass('bg-white');
                    $(this).find('div.info-box:last-child').removeClass('bg-aqua').addClass('bg-white');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('dashboard-widget-hover');

                    //handle dropped item
                    var x = $(this).data('x');
                    var y = $(this).data('y');
                    var uuid = ui.draggable.attr('data-uuid');

                    //need to delay dashboard widget update to avoid js bug (items are destroyed before this function ends!)
                    //delay ust be long enough to allow html template to be downloaded, otherwise dnd won't be enable on it
                    //until other item is moved.
                    setTimeout(function() {
                        self.updateDashboard(uuid, x, y);
                    }, 250);
                }
            });
        });

        //trash droppable
        $('.device-list').each(function()
        {
            $(this).droppable({
                accept: '.dashboard-widget',
                activate: function(event, ui) {
                    $(this).addClass('device-list-active');
                    $(this).find('div.device-list-item').hide();
                },
                deactivate: function(event, ui) {
                    $(this).removeClass('device-list-active');
                    $(this).find('div.device-list-item').show();
                },
                over: function(event, ui) {
                    $(this).addClass('bg-aqua');
                },
                out: function(event, ui) {
                    $(this).removeClass('bg-aqua');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('device-list-hover');
                    $(this).removeClass('bg-aqua');

                    //handle dropped item
                    var uuid = ui.draggable.attr('data-uuid');

                    //remove item from dashboard
                    setTimeout(function()
                    {
                        self.updateDashboard(uuid, -1, -1);
                    }, 100);
                }
            });
        });

        //list items draggable
        $('.device-list-item').each(function()
        {
            $(this).addClass('device-list-item-draggable');
            $(this).draggable({
                opacity: 0.7,
                cursor: 'move',
                revert: 'invalid',
                helper: 'clone',
                appendTo: $('#draganddroparea'),
                containment: $('#draganddroparea'),
                zIndex: 10000
            });
        });

        //dashboard widgets draggable
        $('.dashboard-widget').not('.placeholder').each(function()
        {
            $(this).draggable({
                opacity: 0.7,
                cursor: 'move',
                revert: 'invalid',
                helper: 'clone',
                appendTo: $('#draganddroparea'),
                containment: $('#draganddroparea'),
                zIndex: 10000
            });
        });
    };

    //editor's devices
    self.filteredDevices = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.name && device.name.length>0 )
            {
                if( self.selectedMainFilter()=='rooms' )
                {
                    //filter by rooms
                    if( self.selectedSubFilter() && device.room==self.selectedSubFilter().name )
                    {
                        output.push({uuid:device.uuid, name:device.name, devicetype:device.devicetype});
                    }
                }
                else if( self.selectedMainFilter()=='types' )
                {
                    //filter by devicetype
                    if( self.selectedSubFilter() && device.devicetype==self.selectedSubFilter().name )
                    {
                        output.push({uuid:device.uuid, name:device.name, devicetype:device.devicetype});
                    }
                }
                else
                {
                    //no filter
                    output.push({uuid:device.uuid, name:device.name, devicetype:device.devicetype});
                }
            }

            //apply text filter
            if( self.selectedTextFilter() && $.trim(self.selectedTextFilter())!=='' )
            {
                output = output.filter(function(item) {
                    if( item.name.toLowerCase().indexOf(self.selectedTextFilter())!=-1 )
                    {
                        return true;
                    }
                    if( item.devicetype.toLowerCase().indexOf(self.selectedTextFilter())!=-1 )
                    {
                        return true;
                    }
                    return false;
                });
            }
        }

        //re-enabled drag and drop
        //need to wait sometime to make sure items are inserted
        setTimeout(function()
        {
            self.enableDragAndDrop();
        }, 100);

        return output;
    });

    //clear editor text filter
    self.clearTextFilter = function()
    {
        self.selectedTextFilter('');
        self.enableDragAndDrop();
    };

    //sub filter options
    self.subFilterOptions = ko.computed(function()
    {
        if( self.selectedMainFilter()=='types' )
        {
            //return devices of specified types
            return self.deviceTypes();
        }
        else if( self.selectedMainFilter()=='rooms' )
        {
            //return all rooms
            return self.rooms();
        }
        else
        {
            //return nothing
            return [];
        }
    });

    //show editor
    self.showEditor = function(duration)
    {
        if( duration===undefined )
        {
            duration = 300;
        }

        if( self.editorOpened )
        {
            $('#dashboardEditor').slideUp({
                duration: duration,
                complete: function() {
                },
                always: function() {
                    self.editorOpened = false;
                    self.editLabel('Edit');

                    //close editor, disable edition mode
                    self.disableDragAndDrop();
                }
            });
        }
        else
        {
            $('#dashboardEditor').slideDown({
                duration: duration,
                complete: function() {
                },
                always: function() {
                    self.editorOpened = true;
                    self.editLabel('Close');

                    //editor opened, init draggables and droppables
                    self.enableDragAndDrop();
                }
            });
        }
    };




    //====================================================
    //DASHBOARD

    //build rooms list
    self.rooms = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.rooms().length; i++ )
        {
            var room = self.agocontrol.rooms()[i];
            output.push({uuid:room.uuid, name:room.name, location:room.location});
        }
        return output;
    });

    //build device types
    self.deviceTypes = ko.computed(function()
    {
        var output = [];
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            var device = self.agocontrol.devices()[i];
            if( device.name && device.name.length>0 )
            {
                var found = false;
                for( var j=0; j<output.length; j++ )
                {
                    if( output[j].name==device.devicetype )
                    {
                        found = true;
                        break;
                    }
                }
                if( !found )
                {
                    output.push({uuid:device.devicetype, name:device.devicetype});
                }
            }
        }
        return output;
    });

    //search device in inventory
    self.findDevice = function(uuid)
    {
        for( var i=0; i<self.agocontrol.devices().length; i++)
        {
            if( self.agocontrol.devices()[i].uuid==uuid ) 
            {
                return self.agocontrol.devices()[i];
            }
        }
        return null;
    };

    //return list of devices on page
    self.devicesOnPage = ko.computed(function() {
        var content = [];
        self.dummy(); //trick to force computed re-calculation

        //create array with placeholder
        for( var i=0; i<self.rowNumber(); i++ )
        {
            var line = [];
            for( var j=0; j<self.colNumber(); j++ )
            {
                line.push({devicetype:"placeholder", x:i, y:j});
            }
            content.push(line);
        }

        //fill with custom dashboard devices
        for( var k in self.currentDashboard )
        {
            //drop device not initialized
            if( self.currentDashboard[k]===null || self.currentDashboard[k].x===undefined || self.currentDashboard[k].y===undefined )
            {
                continue;
            }

            //drop device with bad coordinates
            var x = self.currentDashboard[k].x;
            var y = self.currentDashboard[k].y;
            if( x<0 || y<0 || x>=self.rowNumber() || y>=self.colNumber() )
            {
                continue;
            }

            //store device at its position
            var device = self.findDevice(k);
            if( device!==null )
            {
                //device still exists in inventory
                content[x][y] = device;
            }
        }

        //re-enable drag and drop
        setTimeout(function()
        {
            self.enableDragAndDrop();
        }, 250);

        //return flattened array
        return [].concat.apply([], content);
    });

    //update dashboard
    self.updateDashboard = function(uuid, x, y)
    {
        //save update
        var content = {};
        if( x===-1 && y===-1 )
        {
            content.command = "deldevicefloorplan";
        }
        else
        {
            content.command = "setdevicefloorplan";
            content.x = x;
            content.y = y;
        }
        content.floorplan = self.currentDashboard.uuid;
        content.device = uuid;
        content.uuid = self.agocontrol.agoController;
        self.agocontrol.sendCommand(content);

        //update dashboard object
        if( x===-1 && y===-1 )
        {
            delete self.currentDashboard[uuid];
        }
        else
        {
            self.currentDashboard[uuid] = {x:x, y:y};
        }

        //and update dashboard
        self.dummy.notifySubscribers();
    };
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    //init
    var model = new Dashboard(params.dashboard, params.edition, agocontrol);

    model.deviceTemplate = function(item) {
        if( item!==null && agocontrol.supported_devices().indexOf(item.devicetype)!=-1 )
        {
            return 'templates/devices/' + item.devicetype;
        }
        return 'templates/devices/empty';
    }.bind(model);

    model.listDeviceTemplate = function(item) {
        if( agocontrol.supported_devices().indexOf(item.devicetype)!=-1 )
        {
            return 'templates/list/' + item.devicetype;
        }
        return 'templates/list/empty';
    }.bind(model);

    return model;
}

function afterrender_template(model)
{
    if( model && model.showEditor && model.openEditor )
    {
        model.showEditor(0);
    }
}
