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
    self.itemsPerRow = 4;
    self.currentDashboard = dashboard;
    self.dashboardName = ko.observable(ucFirst(self.currentDashboard.name));
    self.editLabel = ko.observable('Edit');
    self.devices = ko.observableArray([]);
    self.selectedTextFilter = ko.observable('');
    self.selectedMainFilter = ko.observable('all');
    self.selectedSubFilter = ko.observable('');
    self.placedDevices = ko.observable([]);
    self.editorOpened = false;

    //after render
    self.afterRender = function()
    {
        if( edition )
        {
            //edition mode, show editor
            self.showEditor(0);
        }
    };



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
                    $(this).find('div.dashboard-widget-header').addClass('dashboard-widget-header-active');
                    $(this).find('div.dashboard-widget-content').addClass('dashboard-widget-content-active');
                },
                deactivate: function(event, ui) {
                    $(this).find('div.dashboard-widget-header').removeClass('dashboard-widget-header-active');
                    $(this).find('div.dashboard-widget-content').removeClass('dashboard-widget-content-active');
                },
                over: function(event, ui) {
                    $(this).find('div.dashboard-widget-header').addClass('dashboard-widget-header-hover');
                    $(this).find('div.dashboard-widget-content').addClass('dashboard-widget-content-hover');
                },
                out: function(event, ui) {
                    $(this).find('div.dashboard-widget-header').removeClass('dashboard-widget-header-hover');
                    $(this).find('div.dashboard-widget-content').removeClass('dashboard-widget-content-hover');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).find('div.dashboard-widget-header').removeClass('dashboard-widget-header-hover');
                    $(this).find('div.dashboard-widget-content').removeClass('dashboard-widget-content-hover');

                    //handle dropped item
                    var x = $(this).data('x');
                    var y = $(this).data('y');
                    var uuid = ui.draggable.attr('data-uuid');

                    //need to delay dashboard widget update to avoid js bug (items are destroyed before this function ends!)
                    //delay must be long enough to allow html template to be downloaded, otherwise dnd won't be enable on it
                    //until other item is moved.
                    setTimeout(function()
                    {
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
                    $(this).addClass('device-list-hover');
                },
                out: function(event, ui) {
                    $(this).removeClass('device-list-hover');
                },
                drop: function(event, ui) {
                    //remove style
                    $(this).removeClass('device-list-hover');

                    //handle dropped item
                    var uuid = ui.draggable.attr('data-uuid');

                    //remove item from dashboard
                    setTimeout(function()
                    {
                        self.updateDashboard(uuid, -1, -1);
                    }, 50);
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
        }, 50);

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
        for( var uuid in self.agocontrol.rooms() )
        {
            output.push({uuid:uuid, name:self.agocontrol.rooms()[uuid].name, location:self.agocontrol.rooms()[uuid].location});
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
            //console.log(device);
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

    //render dashboard
    self.renderDashboard = function()
    {
        //init
        var content = [[null, null, null], [null, null, null], [null, null, null]];
	    self.placedDevices(content);

        if (self.agocontrol.devices().length===0)
        {
            return;
        }

        for ( var k in self.currentDashboard )
        {
            if( self.currentDashboard[k].x===undefined || self.currentDashboard[k].y===undefined )
            {
                //skip uneeded items
                continue;
            }

            var x = self.currentDashboard[k].x;
            var y = self.currentDashboard[k].y;
            if (x < 0 || y < 0)
            {
               continue;
            }

            content[x][y] = self.findDevice(k);
        }

	    self.placedDevices([]);
	    self.placedDevices(content);

        if( self.editorOpened )
        {
            //re-enable drag and drop (dashboard widgets are regenerated)
            setTimeout(function()
            {
                self.enableDragAndDrop();
            }, 50);
        }

    };

    //update dashboard
    self.updateDashboard = function(uuid, x, y)
    {
        //save update
        var content = {};
        content.command = "setdevicefloorplan";
        content.floorplan = self.currentDashboard.uuid;
        content.device = uuid;
        content.uuid = self.agocontrol.agoController;
        content.x = x;
        content.y = y;
        self.agocontrol.sendCommand(content);

        //update dashboard object
        self.currentDashboard[uuid] = {x:x, y:y};

        //and render dashboard
        self.renderDashboard();
    };

    //first row content
    this.firstRow = ko.computed(function()
    {
        var result = [];
        var x = 0;
        for ( var y=0; y<self.itemsPerRow; y++ )
        {
            if( self.placedDevices()[x] && self.placedDevices()[x][y] )
            {
                result.push(self.placedDevices()[x][y]);
            }
            else
            {
                result.push({
                    devicetype : "placeholder",
                    x : x,
                    y : y
                });
            }
        }

        return result;
    });

    //second row content
    this.secondRow = ko.computed(function()
    {
        var result = [];
        var x = 1;
        for ( var y=0; y<self.itemsPerRow; y++ )
        {
            if( self.placedDevices()[x] && self.placedDevices()[x][y] )
            {
                result.push(self.placedDevices()[x][y]);
            }
            else
            {
                result.push({
                    devicetype : "placeholder",
                    x : x,
                    y : y
                });
            }
        }

        return result;
    });

    //third row content
    this.thirdRow = ko.computed(function()
    {
        var result = [];
        var x = 2;
        for ( var y=0; y<self.itemsPerRow; y++ )
        {
            if( self.placedDevices()[x] && self.placedDevices()[x][y] )
            {
                result.push(self.placedDevices()[x][y]);
            }
            else
            {
                result.push({
                    devicetype : "placeholder",
                    x : x,
                    y : y
                });
            }
        }

        return result;
    });

    //render dashboard
    self.renderDashboard();
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    //init
    var model = new Dashboard(params.dashboard, params.edition, agocontrol);

    model.deviceTemplate = function(item) {
        if( agocontrol.supported_devices().indexOf(item.devicetype)!=-1 )
        {
            return 'templates/devices/' + item.devicetype;
        }
        return 'templates/devices/empty';
    }.bind(model);

    model.listDeviceTemplate = function(item) {
        if (agocontrol.supported_devices().indexOf(item.devicetype) != -1)
        {
            return 'templates/list/' + item.devicetype;
        }
        return 'templates/list/empty';
    }.bind(model);

    return model;
}


