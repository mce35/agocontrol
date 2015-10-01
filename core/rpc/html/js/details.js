/**
 * Extend Agocontrol object with device details functions
 */

//Opens details page for the given device
Agocontrol.prototype.showDetails = function(device)
{
    var self = this;

    //get environment
    var environment = null;
    if( device && device.valueList && device.valueList() )
    {
        for( var i=0; i<device.valueList().length; i++ )
        {
            environment = device.valueList()[i].name;
            break;
        }
    }

    //Check if we have a template if yes use it otherwise fall back to default
    $.ajax({
        type : 'HEAD',
        url : "templates/details/" + device.devicetype + ".html",
        success : function()
        {
            self.doShowDetails(device, device.devicetype, environment);
        },
        error : function()
        {
            self.doShowDetails(device, "default", environment);
        }
    });
};

//Shows the detail page of a device
Agocontrol.prototype.doShowDetails = function(device, template, environment)
{
    var self = this;

    ko.renderTemplate("templates/details/" + template, device,
    {
        afterRender : function()
        {
            $('input[type=radio][name=renderType]').on('change', function() {
                self.render(device, environment, $(this).val());
            });

            if( $('#commandList').length )
            {
                //empty template, fill command list
                self.showCommandList($('#commandList')[0], device);
            }
            else if( device.devicetype=='camera' )
            {
                //camera template, display video frame
                device.getVideoFrame();
            }
            else if( ((device.valueList && device.valueList() && device.valueList().length) || device.devicetype == "binarysensor" || device.devicetype=="multigraph"))
            {
                //sensor template
                    
                //add refresh button action
                $('#get_graph').click(function(e) {
                    e.preventDefault();
                    self.render(device, environment);
                });

                //set start date
                var start = new Date((new Date()).getTime() - 24 * 3600 * 1000);
                var startEl = $('#start_date');
                startEl.val($.datepicker.formatDate('dd.mm.yy', start) + ' 00:00');
                startEl.datetimepicker({
                    format: 'd.m.Y H:i',
                    onChangeDateTime: function(dp,$input)
                    {
                        //check date
                        var sd = stringToDatetime($('#start_date').val());
                        var ed = stringToDatetime($('#end_date').val());
                        if( sd.getTime()>ed.getTime() )
                        {
                            //invalid date
                            notif.warning('Specified datetime is invalid');
                            sd = new Date(ed.getTime() - 24 * 3600 * 1000);
                            $('#start_date').val( datetimeToString(sd) );
                        }
                    }
                });

                //set end date
                var endEl = $('#end_date');
                endEl.val($.datepicker.formatDate('dd.mm.yy', new Date())+' 23:59');
                endEl.datetimepicker({
                    format:'d.m.Y H:i',
                    onChangeDateTime: function(dp,$input)
                    {
                        //check date
                        var sd = stringToDatetime($('#start_date').val());
                        var ed = stringToDatetime($('#end_date').val());
                        if( sd.getTime()>ed.getTime() )
                        {
                            //invalid date
                            notif.warning('Specified datetime is invalid');
                            ed = new Date(sd.getTime() + 24 * 3600 * 1000);
                            $('#end_date').val( datetimeToString(ed) );
                        }
                    }
                });

                if( device.devicetype=="binarysensor" || device.devicetype=="multigraph" )
                {
                    environment = "device.state";
                }

                //render default
                if( device.devicetype=="gpssensor" )
                {
                    self.render(device, environment ? environment : device.valueList()[0].name, "map");
                }
                else
                {
                    self.render(device, environment ? environment : device.valueList()[0].name, "graph");
                }
            }
                
            //show modal
            $('#detailsModal').modal('show');
        }
    }, $('#detailsContent')[0]);
};

//Shows the command selector for the detail pages
Agocontrol.prototype.showCommandList = function(container, device)
{
    var self = this;
    var commandSelect = document.createElement("select");
    var commandParams = document.createElement("span");
    commandSelect.id = "commandSelect";
    commandSelect.className = "form-control";
    var type = device.devicetype;
    if( type && self.schema().devicetypes[type] )
    {
        for ( var i = 0; i < self.schema().devicetypes[type].commands.length; i++)
        {
            commandSelect.options[i] = new Option(self.schema().commands[self.schema().devicetypes[type].commands[i]].name, self.schema().devicetypes[type].commands[i]);
        }
    }

    commandSelect.onchange = function()
    {
        if( commandSelect.options.length===0 )
        {
           return 0;
        }
        var cmd = self.schema().commands[commandSelect.options[commandSelect.selectedIndex].value];
        commandParams.innerHTML = "";
        if( cmd.parameters!==undefined )
        {
            commandParams.style.display = "";
            for( var param in cmd.parameters )
            {
                if (cmd.parameters[param].type == 'option')
                {
                    var select = document.createElement("select");
                    select.name = param;
                    select.className = "cmdParam form-control";
                    for ( var i = 0; i < cmd.parameters[param].options.length; i++)
                    {
                        select.options[select.options.length] = new Option(cmd.parameters[param].options[i], cmd.parameters[param].options[i]);
                    }
                    commandParams.appendChild(select);
                }
                else
                {
                    var input = document.createElement("input");
                    input.name = param;
                    input.className = "cmdParam form-control";
                    input.placeholder = cmd.parameters[param].name;
                    commandParams.appendChild(input);
                }
            }
        }
        else
        {
            commandParams.style.display = "none";
        }
    };

    commandSelect.onchange();
   
    container.appendChild(commandSelect);
    container.appendChild(commandParams);
};

//Render plots graph
//Based on D3 sample http://bl.ocks.org/mbostock/4015254
Agocontrol.prototype.renderPlots = function(device, environment, unit, data, startDate, endDate)
{
    //clear container
    $('#graphContainer').empty();

    //check if we have data
    if( data.length==0 )
    {
        notif.warning('No data to display');
        return;
    }

    //prepare y label
    var yLabel = environment;
    if( unit.length>0 )
    {
        yLabel += ' (' + unit + ')';
    }

    //prepare fill color
    var colorL = '#000000';
    var colorA = '#A0A0A0';
    if( device.devicetype=='humiditysensor' )
    {
        colorL = '#0000FF';
        colorA = '#7777FF';
    }
    else if( device.devicetype=='temperaturesensor' )
    {
        colorL = '#FF0000';
        colorA = '#FF8787';
    }
    else if( device.devicetype=='powermeter' || device.devicetype=='energysensor' ||device.devicetype=='powersensor' ||device.devicetype=='' ||device.devicetype=='batterysensor' )
    {
        colorL = '#007A00';
        colorA = '#00BB00';
        
    }
    else if( device.devicetype=='brightnesssensor' )
    {
        colorL = '#CCAA00';
        colorA = '#FFD400';
    }

    //graph parameters
    var margin = {top: 30, right: 20, bottom: 30, left: 50},
        width = 870 - margin.left - margin.right,
        height = 325 - margin.top - margin.bottom;
    var x = d3.time.scale()
        .range([0, width]);
    var y = d3.scale.linear()
        .range([height, 0]);
    var xAxis = d3.svg.axis()
        .scale(x)
        .orient("bottom")
        .tickSize(-height, 0)
        .tickPadding(6);
    var yAxis = d3.svg.axis()
        .scale(y)
        .orient("left")
        .tickSize(-width)
        .tickPadding(6);

    //area generator
    var area = d3.svg.area()
        .interpolate("step-after")
        .x(function(d) { return x(d.date); })
        .y0(y(0))
        .y1(function(d) { return y(d.value); });

    //line generator
    var line = d3.svg.line()
        .interpolate("step-after")
        .x(function(d) { return x(d.date); })
        .y(function(d) { return y(d.value); });

    //create graph canvas
    var svg = d3.select("#graphContainer").append("svg")
            .attr("width", width + margin.left + margin.right)
            .attr("height", height + margin.top + margin.bottom)
            .style("font-size", "10px")
        .append("g")
            .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

    function draw()
    {
        svg.select("g.x.axis").call(xAxis);
        svg.select("g.y.axis").call(yAxis);
        svg.select("path.area").attr("d", area);
        svg.select("path.line").attr("d", line);
    }

    //define zoom
    var zoom = d3.behavior.zoom()
        .on("zoom", draw);

    //configure graph
    svg.append("clipPath")
            .attr("id", "clip")
        .append("rect")
            .attr("x", x(0))
            .attr("y", y(1))
            .attr("width", x(1) - x(0))
            .attr("height", y(0) - y(1));
    svg.append("g")
            .attr("class", "y axis")
        .append("text")
            .attr("transform", "rotate(-90)")
            .attr("y", 6)
            .attr("dy", ".8em")
            .style("text-anchor", "end")
            .text(yLabel);
    svg.append("path")
        .attr("class", "area")
        .attr("clip-path", "url(#clip)")
        .style("fill", colorA);
    svg.append("g")
        .attr("class", "x axis")
        .attr("transform", "translate(0," + height + ")");
    svg.append("path")
        .attr("class", "line")
        .attr("clip-path", "url(#clip)")
        .style("stroke", colorL)
        .style("fill", "none")
        .style("stroke-width", "1.5px");
    svg.append("rect")
        .attr("class", "pane")
        .attr("width", width)
        .attr("height", height)
        .style("fill", "none")
        .style("cursor", "move")
        .style("pointer-events", "all")
        .call(zoom);

    //prepare data
    data.forEach(function(d) {
        d.date = d.time*1000;
        d.value = +d.level;
    });

    //compute boundaries
    x.domain(d3.extent(data, function(d) { return d.date; }));
    y.domain([0, d3.max(data, function(d) { return d.value; })]);

    zoom.x(x);

    //bind the data to our path elements.
    svg.select("path.area").data([data]);
    svg.select("path.line").data([data]);

    //finaly draw graph
    draw();
};

//Render data list
Agocontrol.prototype.renderList = function(device, environment, unit, values, startDate, endDate)
{
    var self = this;
    var data = []

    values.sort(function(a, b) {
        return b.time - a.time;
    });

    if( values.length>0 )
    {
        if( values[0].level )
        {
            for ( var i = 0; i < values.length; i++)
            {
                values[i].date = formatDate(new Date(values[i].time * 1000));
                values[i].value = values[i].level + " " + unit;
                delete values[i].level;
            }
        }
        else if( values[0].latitude && values[0].longitude )
        {
            for ( var i = 0; i < values.length; i++)
            {
                values[i].date = formatDate(new Date(values[i].time * 1000));
                values[i].value = values[i].latitude + "," + values[i].longitude;
                delete values[i].latitude;
                delete values[i].longitude;
            }
        }
    }

    ko.renderTemplate("templates/details/datalist", {
        data : values,
        environment : environment
    }, {}, $('#graphContainer')[0]);
};

//Render map to display GPS positions
Agocontrol.prototype.renderMap = function(values)
{
    var self = this;

    //load openlayers lib only when needed
    head.load('js/libs/OpenLayers/OpenLayers.js', function() {
        //configure openlayers lib
        OpenLayers.ImgPath = 'js/libs/OpenLayers/img/';

        //clear container
        if( values.length>0 )
        {
            //create map, layers and projection
            var map = new OpenLayers.Map('graph');
            var layer = new OpenLayers.Layer.OSM();
            var markers = new OpenLayers.Layer.Markers("Markers");
            var vectors = new OpenLayers.Layer.Vector("Lines");
            var fromProjection = new OpenLayers.Projection("EPSG:4326");
            var toProjection = new OpenLayers.Projection("EPSG:900913");
            var lineStyle = {
                strokeColor: '#FF0000',
                strokeOpacity: 0.5,
                strokeWidth: 5
            };

            //add layers
            map.addLayer(layer);
            map.addLayer(markers);
            map.addLayer(vectors);

            //add markers
            var prevPoint = null;
            var features = [];
            for( var i=0; i<values.length; i++ )
            {
                var position = new OpenLayers.LonLat(values[i].longitude, values[i].latitude).transform(fromProjection, toProjection);
                var point = new OpenLayers.Geometry.Point(values[i].longitude, values[i].latitude).transform(fromProjection, toProjection);
                markers.addMarker(new OpenLayers.Marker(position));
                if( prevPoint )
                {
                    //join markers
                    var line = new OpenLayers.Geometry.LineString([prevPoint, point]);
                    features.push( new OpenLayers.Feature.Vector(line, null, lineStyle) );
                }
                prevPoint = point;
            }
            vectors.addFeatures(features);

            //center map to first position
            var zoom = 13;
            map.setCenter( new OpenLayers.LonLat(values[0].longitude, values[0].latitude).transform(fromProjection, toProjection), zoom);
        }
        else
        {
            notif.warning('No data to display');
        }
    });
};

//Render RRDtool graph
Agocontrol.prototype.renderRRD = function(data)
{
    //prepare html elements
    $('#graphContainer').empty();
    var img = $('<img class="img-responsive" src="data:image/png;base64,'+data+'" id="graphRRD" />');
    $('#graphContainer').append(img);
};

//render stuff
Agocontrol.prototype.render = function(device, environment, type)
{
    var self = this;
    self.block($('#graphContainer'));

    //get type
    if( type===null || type===undefined )
    {
        type = type = self.lastRenderType;
    }
    self.lastRenderType = type;

    var start = stringToDatetime($('#start_date').val());
    var end = stringToDatetime($('#end_date').val());

    //fix end date
    now = new Date();
    if( end.getTime()>now.getTime() )
    {
        end = now;
    }

    if( type=="graph" )
    {
        //RRD graph, get image data
        var content = {};
        content.command = "getgraph";
        content.uuid = self.dataLoggerController;
        content.devices = [device.uuid];
        content.start = Math.round(start.getTime()/1000);
        content.end = Math.round(end.getTime()/1000);
        self.sendCommand(content)
            .then(function(res) {
                self.renderRRD(res.data.graph);
            })
            .catch(function(err) {
                notif.error("Unable to render values");
                console.error(err);
            })
            .finally(function() {
                self.unblock($('#graphContainer'));
            });
    }
    else
    {
        //other render needs values and unit
        var content = {};
        content.uuid = self.dataLoggerController;
        content.command = "getdata";
        content.replytimeout = 15;
        content.deviceid = device.uuid;
        content.start = start.toISOString();
        content.end = end.toISOString();
        content.env = environment.toLowerCase();
        self.sendCommand(content, null, 30)
            .then(function(res) {
                //get unit
                var unit = "";
                for ( var k = 0; k < device.valueList().length; k++)
                {
                    if (device.valueList()[k].name == environment)
                    {
                        unit = device.valueList()[k].unit;
                        break;
                    }
                }

                //get data
                var values = res.data.values;

                //render
                if( type=="plots" )
                {
                    self.renderPlots(device, environment, unit, values, start, end);
                }
                else if( type=="list" )
                {
                    self.renderList(device, environment, unit, values, start, end);
                }
                else if( type=="map" )
                {
                    self.renderMap(values);
                }
            })
            .catch(function(err) {
                notif.error("Unable to render values");
                console.error(err);
            })
            .finally(function() {
                self.unblock($('#graphContainer'));
            });   
    }
};

