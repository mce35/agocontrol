/**
 * Extend Agocontrol object with device details functions
 */

//Opens details page for the given device
Agocontrol.prototype.showDetails = function(device, environment)
{
    var self = this;

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
            self.doShowDetails(device, "default");
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
            var dialogWidth = 800;
            var dialogHeight = 300;

            if (document.getElementById('commandList'))
            {
                self.showCommandList(document.getElementById('commandList'), device);
            }

            if (device.devicetype == "camera")
            {
                dialogHeight = 620;
                device.getVideoFrame();
            }

            if (document.getElementById('graph') && ((device.valueList && device.valueList() && device.valueList().length) || device.devicetype == "binarysensor" || device.devicetype=="multigraph"))
            {
                //refresh button
                $('#get_graph').click(function()
                {
                    self.renderGraph(device, document.getElementById('graph')._environment);
                });
    
                //Setup start date
                var start = new Date((new Date()).getTime() - 24 * 3600 * 1000);
                var startEl = $('#start_date');
                startEl.val($.datepicker.formatDate('dd.mm.yy', start) + ' 00:00');
                startEl.datetimepicker({
                    closeOnDateSelect: true,
                    //value: $.datepicker.formatDate('dd.mm.yy', start) + ' 00:00',
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

                //Setup end date
                var endEl = $('#end_date');
                endEl.val($.datepicker.formatDate('dd.mm.yy', new Date())+' 23:59');
                endEl.datetimepicker({
                    closeOnDateSelect: true,
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
    
                if (device.devicetype == "binarysensor")
                {
                    environment = "device.state";
                }
                else if (device.devicetype == "multigraph")
                {
                    environment = "device.state";
                }

                self.renderGraph(device, environment ? environment : device.valueList()[0].name);
    
                for( var i=0; i<document.getElementsByName("displayType").length; i++ )
                {
                    document.getElementsByName("displayType")[i].onchange = function() {
                        self.renderGraph(device, environment ? environment : device.valueList()[0].name); 
                    }
                }

                var reset = function() {
                    if (device !== undefined)
                    {
                        device.reset();
                    }
                };
                  
                dialogWidth = 1000;
                dialogHeight = 720;
            }

            if (document.getElementById("detailsTitle"))
            {
                $("#detailsPage").dialog({
                    title : document.getElementById("detailsTitle").innerHTML,
                    modal : true,
                    width : Math.min(dialogWidth, Math.round(screen.width * 0.8)),
                    height : Math.min(dialogHeight, Math.round(screen.height * 0.8)),
                    close : function()
                    {
                        var graphContainer = document.getElementById('graph');
                        if (graphContainer)
                        {
                            graphContainer.parentNode.removeChild(graphContainer);
                        }
                    },
                    open : function()
                    {
                        $("#detailsPage").css("overflow", "visible");
                        if (device.dialogopened !== undefined)
                            device.dialogopened(this);
                    }
                });
            }
        }
    }, document.getElementById("detailsPage"));
};

//Shows the command selector for the detail pages
Agocontrol.prototype.showCommandList = function(container, device)
{
    var self = this;
    var commandSelect = document.createElement("select");
    var commandParams = document.createElement("span");
    commandSelect.id = "commandSelect";
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
        if (commandSelect.options.length == 0)
        {
           return 0;
        }
        var cmd = self.schema().commands[commandSelect.options[commandSelect.selectedIndex].value];
        commandParams.innerHTML = "";
        if (cmd.parameters !== undefined)
        {
            commandParams.style.display = "";
            for ( var param in cmd.parameters)
            {
                if (cmd.parameters[param].type == 'option')
                {
                    var select = document.createElement("select");
                    select.name = param;
                    select.className = "cmdParam";
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
                    input.className = "cmdParam";
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
Agocontrol.prototype.renderPlots = function(device, environment, unit, data, values, startDate, endDate)
{
    var self = this;

    //need data to render something else
    var max_ticks = 25; // User option?

    //Split the values into buckets
    var num_buckets = Math.max(1, Math.floor(values.length / max_ticks));
    var buckets = values.chunk(num_buckets);
    var labels = [];
    var i = 0;

    //Compute average for each bucket and pick a representative time to display
    for ( var j = 0; j < buckets.length; j++)
    {
        var bucket = buckets[j];
        var ts = bucket[0].time + (bucket[bucket.length - 1].time - bucket[0].time) / 2;
        labels.push(new Date(Math.floor(ts) * 1000));
        var value = 0;
        for ( var k = 0; k < bucket.length; k++)
        {
            value += bucket[k].level;
        }
        data.push([ i, value / k ]);
        i++;
    }

    //Render the graph
    var container = document.getElementById('graph');
    container._environment = environment;
    Flotr.draw(container, [ data ], {
        HtmlText : false,
        title : environment,
        mode : "time",
        yaxis : {
            tickFormatter : function(x) {
                return x + " " + unit;
            },
        },
        mouse : {
            track : true,
            relative : true,
            trackFormatter : function(o) {
                return formatDate(labels[Math.round(o.x)]) + " - " + o.y + " " + unit;
            }
        },
        xaxis : {
            noTicks : i,
            labelsAngle : 90,
            tickDecimals : 0,
            tickFormatter : function(x) {
                return formatDate(labels[x]);
            }
        }
    });

    //We have no data ...
    if (data.length == 0)
    {
        var canvas = document.getElementsByClassName("flotr-overlay")[0];
        var context = canvas.getContext("2d");
        var x = canvas.width / 2;
        var y = canvas.height / 2;

        context.font = "30pt Arial";
        context.textAlign = "center";
        context.fillStyle = "red";
        context.fillText('No data found for given time frame!', x, y);
    }

    self.unblock($("#graph"));
};

//Render data list
Agocontrol.prototype.renderList = function(device, environment, unit, data, values, startDate, endDate)
{
    var self = this;

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
    }, {}, document.getElementById("dataList"));

    self.unblock($("#dataList"));
};

//Render map to display GPS positions
Agocontrol.prototype.renderMap = function(values)
{
    var self = this;

    //load openlayers lib only when needed
    yepnope({
        load : 'js/libs/OpenLayers/OpenLayers.js',
        complete : function() {
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
                notif.error('No data to display');
            }
            //show container
            self.unblock($("#graph"));
        }
    });
};

//Render RRDtool graph
Agocontrol.prototype.renderRRDgraph = function(device, startDate, endDate)
{
    var self = this;

    //fix end date
    now = new Date();
    if( endDate.getTime()>now.getTime() )
    {
        endDate = now; 
    }

    //prepare content
    $('#graph').empty();
    var img = $('<img src="" id="graphRRD" style="display: none"/>');
    $('#graph').append(img);

    //get graph
    device.getRrdGraph(device.uuid, Math.round(startDate.getTime()/1000), Math.round(endDate.getTime()/1000));
    self.unblock($("#graph"));
};

//Renders the graph for the given device and environment
Agocontrol.prototype.renderGraph = function(device, environment)
{
    var self = this;
    var renderType = $($('input[name="displayType"]:checked')[0]).val();
    var endDate = stringToDatetime($('#end_date').val());
    var startDate = stringToDatetime($('#start_date').val());
    var renderType = $($('input[name="displayType"]:checked')[0]).val();

    self.block($("#graph"));

    if( renderType=="graph" )
    {
        //render rrdtool graph
        $("#graph").show();
        $("#dataList").hide();
        self.renderRRDgraph(device, startDate, endDate);
    }
    else
    {
        var content = {};
        content.uuid = dataLoggerController;
        content.command = "getdata";
        content.deviceid = device.uuid;
        content.start = startDate.toISOString();
        content.end = endDate.toISOString();
        content.env = environment.toLowerCase();

        self.sendCommand(content, function(res) {
            if (!res.result || !res.result.result || !res.result.result.values)
            {
                notif.fatal("Error while loading Graph!");
                self.unblock($('#graph'));
                return;
            }

            //Get the unit
            var unit = "";
            for ( var k = 0; k < device.valueList().length; k++)
            {
                if (device.valueList()[k].name == environment)
                {
                    unit = device.valueList()[k].unit;
                    break;
                }
            }

            //Prepare the data
            var data = [];
            var values = res.result.result.values;

            if (renderType == "list")
            {
                $("#graph").hide();
                $("#dataList").show();
                self.block($("#dataList"));
                self.renderList(device, environment, unit, data, values, startDate, endDate);
                self.unblock($("#dataList"));
            }
            else if( renderType=="map" )
            {
                $("#graph").show();
                $("#dataList").hide();
                self.block($("#graph"));
                self.renderMap(valuest);
                self.unblock($("#graph"));
            }
            else if( renderType=="plots" )
            {
                $("#graph").show();
                $("#dataList").hide();
                self.block($("#graph"));
                self.renderPlots(device, environment, unit, data, values, startDate, endDate);
                self.unblock($("#graph"));
            }
        }, 30);
    }
};

