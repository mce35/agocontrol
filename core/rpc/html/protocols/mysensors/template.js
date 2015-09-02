/**
 * MySensors plugin
 */
function MySensors(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.mysensorsControllerUuid = null;
    self.port = ko.observable();
    self.devices = ko.observableArray();
    self.selectedRemoveDevice = ko.observable();
    self.selectedCountersDevice = ko.observable();
    self.counters = ko.observableArray([]);
    self.countersGrid = new ko.agoGrid.viewModel({
        data: self.counters,
        columns: [
            {headerText: 'Device', rowText:'device'},
            {headerText: 'Sent', rowText:'counter_sent'},
            {headerText: 'Failed', rowText:'counter_failed'},
            {headerText: 'Received', rowText:'counter_received'},
            {headerText: 'Last message', rowText:'last_timestamp'}
        ],
        rowTemplate: 'countersRowTemplate',
        pageSize: 25
    });
    self.graphBuilt = false;

    //MySensor controller uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='mysensorscontroller' )
            {
                self.mysensorsControllerUuid = devices[i].uuid;
                break;
            }
        }
    }

    //set port
    self.setPort = function() {
        //first of all unfocus element to allow observable to save its value
        $('#setport').blur();
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'setport',
            port: self.port()
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.error==0 )
                {
                    notif.success('#sp');
                }
                else 
                {
                    //error occured
                    notif.error(res.result.msg);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get port
    self.getPort = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getport'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.port(res.result.port);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //reset all counters
    self.resetAllCounters = function() {
        if( confirm("Reset all counters?") )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'resetallcounters'
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //reset counters
    self.resetCounters = function() {
        if( confirm("Reset counters of selected device?") )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'resetcounters',
                device: self.selectedCountersDevice()
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get devices list
    self.getDevices = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getdevices'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.devices(res.result.devices);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //remove device
    self.removeDevice = function() {
        if( confirm('Delete device?') )
        {
            var content = {
                uuid: self.mysensorsControllerUuid,
                command: 'remove',
                device: self.selectedRemoveDevice()
            }
    
            self.agocontrol.sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('#ds');
                        //refresh devices list
                        self.getDevices();
                        self.getCounters();
                    }
                    else 
                    {
                        //error occured
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get counters
    self.getCounters = function() {
        var content = {
            uuid: self.mysensorsControllerUuid,
            command: 'getcounters'
        }

        self.agocontrol.sendCommand(content, function(res)
        {
            var counters = [];
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                for( device in res.result.counters )
                {
                    counters.push(res.result.counters[device]);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
            self.counters(counters);
        });
    };

    //build route graph
    self.buildGraph = function(container) {
        if( self.graphBuilt )
        {
            //graph already built. Stop here
            return;
        }
        
        var color = d3.scale.category10();
        var width = 750;
        var height = 500;

        //build nodes
        var gateway = {'id':0, 'name':'Gateway', 'type':'Gateway', 'links':0, 'x':width/2, 'y':height/2, 'fixed':true};
        var nodes = [gateway];
        var existingNodes = {'0':gateway};
        for( var i=0; i<self.counters().length; i++ )
        {
            //split device name ("xxx/xxx (xxx)") to get nodeid
            var nodeid = self.counters()[i]['device'].split("/")[0];
            if( existingNodes[nodeid]===undefined )
            {
                var type = 'Sensor';
                if( self.counters()[i]['type']==='networkrelay' )
                {
                    type = 'Repeater';
                }
                existingNodes[nodeid] = {'id':nodeid, 'name':type+' #'+nodeid, 'type':type, 'links':0};
                nodes.push(existingNodes[nodeid]);
            }
        }

        //build links
        var links = [];
        var existingLinks = [];
        for( var i=0; i<self.counters().length; i++ )
        {
            if( self.counters()[i]['last_route']!==undefined )
            {
                //split device name ("xxx/xxx (xxx)") to get nodeid
                var nodeid = self.counters()[i]['device'].split("/")[0];

                //split route: #0=origin #1=repeater #2=destination
                var route = self.counters()[i]['last_route'].split("->");
                if( existingLinks.indexOf(self.counters()[i]['last_route'])===-1 )
                {
                    if( route[0]==route[1] && existingNodes[route[0]]!==undefined && existingNodes[route[2]]!==undefined )
                    {
                        //no repeater
                        links.push({"source":existingNodes[route[0]], "target":existingNodes[route[2]]});
                        existingNodes[route[0]]['links']++;
                        existingNodes[route[1]]['links']++;
                        existingLinks.push(self.counters()[i]['last_route']);
                    }
                    else if( existingNodes[route[0]]!==undefined && existingNodes[route[1]]!==undefined && existingNodes[route[2]]!==undefined )
                    {
                        //repeater
                        links.push({"source":existingNodes[route[0]], "target":existingNodes[route[1]]});
                        links.push({"source":existingNodes[route[1]], "target":existingNodes[route[2]]});
                        existingNodes[route[0]]['links']++;
                        existingNodes[route[1]]['links']++;
                        existingNodes[route[2]]['links']++;
                        existingLinks.push(self.counters()[i]['last_route']);
                    }
                }
            }
        }

        $(container).empty();
        var svg = d3.select(container).append('svg')
            .attr('width', width)
            .attr('height', height);

        var force = d3.layout.force()
            .size([width, height])
            .nodes(nodes)
            .links(links)
            .charge(function(d) {
                var charge = -250;
                if( d.type==='Gateway' || d.type==='Repeater' )
                {
                    return 10*charge;
                }
                else if( d.links===0 )
                {
                    return charge;
                }
                else
                {
                    return 5*charge;
                }
            });

        var link = svg.selectAll('.link')
            .data(links)
            .enter().append('line')
            .attr('class', 'link');

        var node = svg.selectAll('.node')
            .data(nodes)
            .enter().append('circle')
            .attr('class', function(d) {
                if( d.links===0 )
                {
                    return 'node node-notconnected';
                }
                if( d.type==='Sensor' )
                {
                    return 'node node-sensor';
                }
                else if( d.type==='Repeater' )
                {
                    return 'node node-repeater';
                }
                else if( d.type==='Gateway' )
                {
                    return 'node node-gateway';
                }
                else
                {
                    return 'node';
                }
            });

        node.append('title')
            .text(function(d) { return d.name; });

        force.on('tick', function() {
            node.attr('r', 15)
                .attr('cx', function(d) { return d.x; })
                .attr('cy', function(d) { return d.y; });

            link.attr('x1', function(d) { return d.source.x; })
                .attr('y1', function(d) { return d.source.y; })
                .attr('x2', function(d) { return d.target.x; })
                .attr('y2', function(d) { return d.target.y; });

        });

        force.start();
        self.graphBuilt = true;
    };

    //new tab selected
    self.onTabChanged = function(index, name)
    {
        if( index==1 && self.counters().length>0 )
        {
            self.buildGraph("#network");
        }
    };

    //force graph redrawing
    self.redrawGraph = function() {
        self.graphBuilt = false;
        self.buildGraph('#network');
    };

    //init ui
    self.getPort();
    self.getDevices();
    self.getCounters();
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new MySensors(agocontrol.devices(), agocontrol);
    return model;
}

