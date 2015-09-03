/**
 * Z-Wave plugin
 */
 
function Zwave(devices, agocontrol)
{
    //members
    var self = this;
    self.devices = devices;
    self.agocontrol = agocontrol;
    self.controllerUuid = null;
   
    /********
     * GRAPHS
     ********/
    self.buildChordGraph = function(content, nodes, onNodeDetails) {
        //code from: http://bl.ocks.org/AndrewRP/7468330
        //click example: http://jsfiddle.net/mdml/K6FHW/
        var width = 1100;
        var height = 1100;
        var outerRadius = Math.min(width, height) / 2 - 10;
        var innerRadius = outerRadius - 24;
        var titles = [];
        var ids = [];
        var matrix = [];
        var count = nodes.length;
        var found;
        for( var i=0; i<count; i++ )
        {
            var nodeName = nodes[i].type;
            if( nodes[i].product.length>0 )
            {
                //replace node type with node product info
                nodeName = nodes[i].product;
            }
            if( nodes[i].manufacturer.length>0 )
            {
                //and append manufacturer if available
                nodeName += ' (' + nodes[i].manufacturer + ')';
            }
            titles.push(""+nodeName+" ("+(i+1)+")");
            //ids.push(""+(i+1));
            ids.push(nodes[i].id);
            var deps = [];
            for( var j=0; j<count; j++ )
            {
                found = false;
                for( var k=0; k<nodes[i].neighbors.length; k++ )
                {
                    if( k==j )
                    {
                        deps.push(1);
                        found = true;
                        break;
                    }
                }
                if( !found )
                {
                    deps.push(0);
                }
            }
            //connect node to itself to make it visible in graph in case of no dependency
            deps[i] = 0.5;
            matrix.push(deps);
        }

        var colors = d3.scale.category20c();

        var arc = d3.svg.arc()
                    .innerRadius(innerRadius)
                    .outerRadius(outerRadius);

        var layout = d3.layout.chord()
                       .padding(.04)
                       .sortSubgroups(d3.descending)
                       .sortChords(d3.ascending);

        var path = d3.svg.chord()
                     .radius(innerRadius);

        var svg = d3.select(content).append("svg")
                    .attr("width", width)
                    .attr("height", height)
                    .append("g")
                    .attr("id", "circle")
                    .attr("transform", "translate(" + width / 2 + "," + height / 2 + ")");

        svg.append("circle")
           .attr("r", outerRadius);

        // Compute the chord layout.
        layout.matrix(matrix);

        // Add a group per neighborhood.
        var container = svg.append("g");
        var group = container.selectAll(".group")
                    .data(layout.groups)
                    .enter().append("g")
                    .attr("class", "group")
                    .on("mouseover", mouseover)
                    .on("click", function(d){
                        onNodeDetails(d.index);
                    });

        // Add a mouseover title.
        group.append("title").text(function(d, i) {
            return titles[i];
        });

        // Add the group arc.
        var groupPath = group.append("path")
                             .attr("id", function(d, i) { return "group" + i; })
                             .attr("d", arc)
                             .style("fill", function(d, i) { return colors(i); });

        // Add a text label.
        var groupText = group.append("text")
                             .attr("x", 6)
                             .attr("dy", 15);

        groupText.append("textPath")
                 .attr("xlink:href", function(d, i) { 
                     return "#group" + i;
                 })
                 .text(function(d, i) { return ids[i]; });

        // Replace labels (device type+id) that don't fit with node id only
        groupText.filter(function(d, i) {
            return groupPath[0][i].getTotalLength() / 2 - 16 < this.getComputedTextLength();
        }).remove();

        // Add the chords.
        var chord = container.selectAll(".chord")
                       .data(layout.chords)
                       .enter().append("path")
                       .attr("class", "chord")
                       .style("fill", function(d) { return colors(d.source.index); })
                       .attr("d", path);

        // Add an elaborate mouseover title for each chord.
        chord.append("title").text(function(d) {
            var out = "";
            if( d.source.index!==d.target.index )
            {
                out += titles[d.source.index];
                if( matrix[d.target.index][d.source.index]==1 )
                    out += "<-";
                if( matrix[d.source.index][d.target.index]==1 )
                    out += "->";
                out += titles[d.target.index];
            }
            else
            {
                out = "Internal link";
            }
            return out;
        });

        function mouseover(d, i) {
            chord.classed("fade", function(p) {
                return p.source.index!=i && p.target.index!=i;
            });
        }
    };

    //Build directed graph
    self.buildDirectedGraph = function(content, zwnodes, onNodeDetails) {
        var nodes = [];
        var links = [];
        var selectedNode = null;
        var selectedLink = null;
        var posX = 10;
        var posY = 10;
        var paperWidth = 920;
        var paperHeight = 750;
        var boundaries = g.rect(0, 0, paperWidth, paperHeight);

        //init graph
        var graph = new joint.dia.Graph();
        var paper = new joint.dia.Paper({
            el: $(content),
            width: paperWidth,
            height: paperHeight,
            gridSize: 10,
            model: graph
        });

        //*********
        //Functions
        //*********
        
        //Workaround for a bug in jointjs lib:
        // When updating element style using attr() function, links are updated too
        // and moved automatically that is really really boring!
        // Workaround is to apply a CSS style to elements rect
        var setNodeStyle = function(node, style) {
            //init
            var el = V(paper.findViewByModel(node).el);

            //apply style
            el.addClass('rect-'+style);
            if( style!='normal' && el.hasClass('rect-normal') )
                el.removeClass('rect-normal');
            if( style!='error' && el.hasClass('rect-error') )
                el.removeClass('rect-error');
            if( style!='warning' && el.hasClass('rect-warning') )
                el.removeClass('rect-warning');
            if( style!='selected' && el.hasClass('rect-selected') )
                el.removeClass('rect-selected');
        };

        //highlight specified node and its links
        var highlightNode = function(node) {
            setNodeStyle(node, "selected");
            for( var i=0; i<node.zwlinks.length; i++ )
            {
                node.zwlinks[i].attr('.connection/stroke', '#3085D6');
                node.zwlinks[i].toFront();
            }
            selectedNode = node;
        };
        
        //unhighlight node and its links
        var unhighlightNode = function() {
            if( selectedNode )
            {
                setNodeStyle(selectedNode, "normal");
                for( var i=0; i<selectedNode.zwlinks.length; i++ )
                {
                    selectedNode.zwlinks[i].attr('.connection/stroke', '#333333');
                }
                selectedNode = null;
            }
        };
        
        /*
        //highlight specified link and its connected nodes
        var highlightLink = function(link) {
            link.attr('.connection/stroke', '#3085D6');
            link.toFront();
            if( link.attributes.source.obj )
                setNodeStyle(link.attributes.source.obj, "selected");
            if( link.attributes.target.obj )
                setNodeStyle(link.attributes.target.obj, "selected");
            selectedLink = link;
        };
        
        //unhighlight specified link and its connected nodes
        var unhighlightLink = function() {
            if( selectedLink )
            {
                selectedLink.attr('.connection/stroke', '#333333');
                if( selectedLink.attributes.source.obj )
                    setNodeStyle(selectedLink.attributes.source.obj, "normal");
                if( selectedLink.attributes.target.obj )
                    setNodeStyle(selectedLink.attributes.target.obj, "normal");
                selectedLink = null;
            }
        };*/

        //Compute real text width
        //@info http://stackoverflow.com/a/21015393/3333386
        var getTextWidth = function(text, font) {
            var canvas = getTextWidth.canvas || (getTextWidth.canvas = document.createElement("canvas"));
            var context = canvas.getContext("2d");
            context.font = font;
            var metrics = context.measureText(text);
            return metrics.width;
        };
        
        //check if link exists
        var linkExists = function(node1, node2) {
            var i;
            for( i=0; i<node1.zwlinks.length; i++ )
            {
                if( node1.zwlinks[i].zwsource==node1.zwid && node1.zwlinks[i].zwtarget==node2.zwid )
                {
                    return true;
                }
            }
            for( i=0; i<node2.zwlinks.length; i++ )
            {
                if( node2.zwlinks[i].zwsource==node2.zwid && node2.zwlinks[i].zwtarget==node1.zwid )
                {
                    return true;
                }
            }
            return false;
        };
        
        //Computed node positions
        var computePositions = function(nodeNumber, rect) {
            //init
            var sides = [0, 0, 0, 0]; //left, top, right, bottom
            var positions = [];
            var i, j;

            //compute number of nodes per side
            var node = Math.floor(nodeNumber/4);
            for( i=0; i<4; i++ )
            {
                sides[i] = node;
            }
            for( i=0; i<(nodeNumber-(node*4)); i++ )
            {
                sides[i]++;
            }

            //compute positions
            var step = 0;
            var offsetX = 120;
            var offsetY = 40;
            //left
            step = Math.floor((rect.height-offsetY*2)/(sides[0]+1));
            for( j=1; j<=sides[0]; j++ )
            {
                positions.push( {x:offsetX, y:(step*j+offsetY)} );
            }
            //top
            step = Math.floor((rect.width-offsetX*2)/(sides[1]+1));
            for( j=1; j<=sides[1]; j++ )
            {
                positions.push( {x:(step*j+offsetX), y:offsetY} );
            }
            //right
            step = Math.floor((rect.height-offsetY*2)/(sides[2]+1));
            for( j=1; j<=sides[2]; j++ )
            {
                positions.push( {x:(rect.width-offsetX), y:(step*j+offsetY)} );
            }
            //bottom
            step = Math.floor((rect.width-offsetX*2)/(sides[3]+1));
            for( j=1; j<=sides[3]; j++ )
            {
                positions.push( {x:(step*j+offsetX), y:(rect.height-offsetY)} );
            }
            
            return positions;
        };

        //*********
        //UI events
        //*********
        
        //on mouse click on paper
        paper.on('blank:pointerdown', function() {
            unhighlightNode();
            //unhighlightLink();
        });
        
        //on mouse click over a graph element
        paper.on('cell:pointerdown', function(cellView, evt, x, y) { 
            if( cellView && cellView.model && cellView.model.attributes && cellView.model.attributes.type )
            {
                //unhighlight everything
                unhighlightNode();
                //unhighlightLink();
                
                //highlight element
                if( cellView.model.attributes.type=="link" )
                {
                    //link selected
                    //highlightLink(cellView.model);
                }
                else
                {
                    //node selected
                    highlightNode(cellView.model);
                }
            }
        });
        
        //on mouse double click over a graph element
        paper.on('cell:pointerdblclick', function(cellView, evt, x, y) {
            if( cellView && cellView.model && cellView.model.attributes && cellView.model.attributes.type )
            {
                if( cellView.model.attributes.type=="basic.Rect" )
                {
                    //open node detail popup
                    onNodeDetails(cellView.model.zwid-1);
                }
            }
        });
        
        //prevent from links to be overlapped
        graph.on('change:position', function(node) {
            for( var i=0; i<links.length; i++ )
            {
                if( links[i].zwsource!=node.zwid && links[i].zwtarget!=node.zwid )
                {
                    paper.findViewByModel(links[i]).update();
                }
            }
        });

        //*********
        //Objects
        //*********
        
        //create node object
        var node = function(name, zwid, x, y, neighbors) {
            //init
            var text = name+'['+zwid+']';
            var width = getTextWidth(text, "12px verdana") + 40;
            var height = 40;
            
            //adjust positions according to node box size
            x = Math.floor(x - (width/2) );
            y = Math.floor(y - (height/2) );
            
            //create node element
            var n = new joint.shapes.basic.Rect({
                position: { x: x, y: y },
                size: { width: width, height: height },
                attrs: {
                    //remove default style to allow css
                    rect: {
                        fill:'',
                        stroke:'',
                        rx: 3,
                        ry: 5,
                        'stroke-width': 2
                    },
                    text: {
                        text: text,
                        'font-size': 12,
                        stroke: '#FFFFFF',
                        'font-weight': 'normal',
                        'font-family': 'verdana'
                    }
                }
            });
            
            //add custom infos
            n.zwname = name;
            n.zwid = zwid;
            n.zwneighbors = neighbors;
            n.zwlinks = [];
            
            return n;
        };

        //create link object
        var link = function(source, target) {
            //create link element
            var l = new joint.dia.Link({
                source: { 
                    id: source.id, 
                    obj: source
                },
                target: {
                    id: target.id,
                    obj: target
                },
                router: { name: 'metro' },
                connector: { name: 'rounded' },
                attrs: {
                    '.connection': {
                        stroke: '#333333',
                        'stroke-width': 3
                    },
                    //disable link to be deleted
                    '.link-tools': {
                        display: 'none'
                    },
                    //disable link to be unplugged
                    '.marker-arrowheads': {
                        display: 'none'
                    }
                }
            });
            
            //add custom infos
            l.toBack();
            l.zwsource = source.zwid;
            l.zwtarget = target.zwid;
            source.zwlinks.push(l);
            target.zwlinks.push(l);
            
            return l;
        };

        //*********
        //Process
        //*********

        //compute nodes positions
        var positions = computePositions(zwnodes.length, boundaries);
        var i;
        
        //create nodes elements
        for( i=0; i<zwnodes.length; i++ )
        {

            var n = node(zwnodes[i].type, zwnodes[i].id, positions[i].x, positions[i].y, zwnodes[i].neighbors);
            nodes.push(n);
        }
        
        //create links elements
        for( i=0; i<zwnodes.length; i++ )
        {
            for( var j=0; j<nodes[i].zwneighbors.length; j++ )
            {
                //check for duplicates
                if( !linkExists(nodes[i], nodes[nodes[i].zwneighbors[j]-1]) )
                {
                    links.push( link(nodes[i], nodes[nodes[i].zwneighbors[j]-1]) );
                }
            }
        }
        
        //add all elements to graph
        graph.addCells(nodes).addCells(links);

        //apply css style
        for( i=0; i<nodes.length; i++ )
        {
            setNodeStyle(nodes[i], "normal");
        }

        //TODO save elements position (using JSON)
        //var json = graph.toJSON();
    };
   

    /******************
     * GENERAL COMMANDS
     ******************/

    //Get controller uuid
    self.getControllerUuid = function() {
        var uuid = null;
        if( self.devices!==undefined )
        {
            for( var i=0; i<self.devices.length; i++ )
            {
                if( self.devices[i].devicetype=='zwavecontroller' )
                {
                    uuid = self.devices[i].uuid;
                    //console.log('uuid='+self.zwaveControllerUuid);
                    break;
                }
            }
        }
        return uuid;
    };
    
    //Set controller uuid
    self.setControllerUuid = function(uuid) {
        self.controllerUuid = uuid;
    };

    /********************
     * CONTROLER COMMANDS
     ********************/

    //Get nodes
    self.getNodes = function(callback) {
        var content = {
            uuid: self.controllerUuid,
            command: 'getnodes'
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                callback(res.result.data.nodelist);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Get statistics
    self.getStatistics = function(callback) {
        var content = {
            uuid: self.controllerUuid,
            command: 'getstatistics'
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                callback(res.result.data.statistics);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Reset controller
    self.reset = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'reset'
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#resetok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Add node
    self.addNode = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'addnode'
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#addnodeok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Remove specified node
    self.removeNode = function(node) {
        var content = {
            uuid: self.controllerUuid,
            command: 'removenode'
        };
        if( node )
            content.node = node;

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#removenodeok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });

    };

    //Test network
    self.testNetwork = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'testnetwork',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#testnetworkok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Cancel last command
    self.cancel = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'cancel',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#cancelok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Save config
    self.saveConfig = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'saveconfig',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#saveconfigok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Download config
    self.downloadConfig = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'downloadconfig',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#downloadconfigok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Turn on all nodes
    self.allOn = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'allon',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#allonok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Turn off all ndoes
    self.allOff = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'alloff',
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#alloffok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Heal entire network
    self.healNetwork = function() {
        var content = {
            uuid: self.controllerUuid,
            command: 'healnetwork'
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('Heal network ok');
            }
            else
            {
                notif.error('Heal network KO');
            }
        });
    };

    /***************
     * NODE COMMANDS
     ***************/

    //Test node
    self.testNode = function(node) {
        var content = {
            uuid: self.controllerUuid,
            command: 'testnode',
            node: node
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.result )
                {
                    notif.success('#testnodeok');
                }
                else
                {
                    notif.error('#testnodeko');
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Refresh node
    self.refreshNode = function(node) {
        var content = {
            uuid: self.controllerUuid,
            command: 'refreshnode',
            node: node
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.result!==undefined && res.result.result!==-1 )
                {
                    notif.success('#refreshnodeko');
                }
                else
                {
                    notif.error('#refreshnodeko');
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Heal node
    self.healNode = function(node) {
        var content = {
            uuid: self.controllerUuid,
            command: 'healnode',
            node: node
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#healnodeok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Set parameter
    self.setParameter = function(nodeId, commandClassId, index, value) {
        var content = {
            uuid: self.controllerUuid,
            command: 'setconfigparam',
            node: nodeId,
            commandclassid: commandClassId,
            index: index,
            value: value
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.result!==undefined && res.result.result!==-1 )
                {
                    notif.success('#setconfigparamok');
                }
                else 
                {
                    //error occured
                    notif.error('#setconfigparamko');
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Request all configuration parameters
    self.requestAllConfigParams = function(node)
    {
        var content = {
            uuid: self.controllerUuid,
            command: 'requestallconfigparams',
            node: node
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.result!==undefined && res.result.result!==-1)
                {
                    notif.success('#requestparamsok');
                }
                else 
                {
                    //error occured
                    notif.error('#requestparamsko');
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });

    }

    /***********************
     * ASSOCIATIONS COMMANDS
     ***********************/

    //Get associations
    self.getAssociations = function(node, group, callback) {
        var content = {
            uuid: self.controllerUuid,
            command: 'getassociations',
            node: node,
            group: group
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                callback(res.result.data.associations, res.result.data.label, node, group);
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Add association
    self.addAssociation = function(node, group, target) {
        var content = {
            uuid: self.controllerUuid,
            command: 'addassociation',
            node: node,
            group: group,
            target: target
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('#addassociationok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //Remove association
    self.removeAssociation = function(node, group, target) {
        var content = {
            uuid: self.controllerUuid,
            command: 'removeassociation',
            node: node,
            group: group,
            target: target
        };

        self.agocontrol.sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                notif.success('removeassociationok');
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };
}
 
/**
 * Configuration Zwave model
 */
function zwaveConfig(zwave) {
    //members
    var self = this;
    self.zwave = zwave;
    self.nodes = ko.observableArray([]);
    self.nodesCount = ko.observable(-1); //-1 means loading data
    self.port = ko.observable();
    self.nodeInfos = ko.observableArray([]);
    self.nodeStatus = ko.observableArray([]);
    self.nodeParameters = ko.observableArray([]);
    self.nodeAssociations = ko.observableArray([]);
    self.nodesForAssociation = ko.observableArray([]);
    self.stats = ko.observableArray([]);
    self.selectedNodeForAssociation = ko.observable();
    self.selectedNode = null;
    self.nodeInfosGrid = new ko.agoGrid.viewModel({
        data: self.nodeInfos,
        columns: [
            {headerText:'Info', rowText:'info'},
            {headerText:'Value', rowText:'value'}
        ],
        rowTemplate: 'nodeInfosRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });
    self.nodeStatusGrid = new ko.agoGrid.viewModel({
        data: self.nodeStatus,
        columns: [
            {headerText:'Status', rowText:'status'},
            {headerText:'Value', rowText:'value'}
        ],
        rowTemplate: 'nodeStatusRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });
    self.nodeAssociationsGrid = new ko.agoGrid.viewModel({
        data: self.nodeAssociations,
        columns: [
            {headerText:'Status', rowText:'status'},
            {headerText:'Value', rowText:'value'}
        ],
        rowTemplate: 'nodeAssociationsRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });
    self.statsGrid = new ko.agoGrid.viewModel({
        data: self.stats,
        columns: [
            {headerText:'Stat', rowText:'stat'},
            {headerText:'Value', rowText:'value'}
        ],
        rowTemplate: 'statsRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });
    self.nodeParametersGrid = new ko.agoGrid.viewModel({
        data: self.nodeParameters,
        columns: [
            {headerText:'', rowText:''},
            {headerText:'Parameter', rowText:''},
            {headerText:'Value', rowText:''},
            {headerText:'', rowText:''}
        ],
        rowTemplate: 'nodeParametersRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });
    self.nodesGrid = new ko.agoGrid.viewModel({
        data: self.nodes,
        columns: [
            {headerText:'Id', rowText:'id'},
            {headerText:'Device', rowText:'manufacturer'},
            {headerText:'Type', rowText:'type'},
            {headerText:'Awake', rowText:'awake'},
            {headerText:'Failed', rowText:'failed'}
        ],
        rowTemplate: 'nodesRowTemplate',
        displayRowCount: false,
        displaySearch: false,
        displayPagination: false
    });

    //get and set zwave controller uuid
    var zwaveControllerUuid = zwave.getControllerUuid();
    zwave.setControllerUuid(zwaveControllerUuid);

    //open node details
    self.openNodeDetails = function(nodeId) {
        //init
        var i;

        //clear existing infos
        while( self.nodeInfos().length>0 )
            self.nodeInfos.pop();
        while( self.nodeStatus().length>0 )
            self.nodeStatus.pop();
        while( self.nodeParameters().length>0 )
            self.nodeParameters.pop();
        while( self.nodeAssociations().length>0 )
            self.nodeAssociations.pop();
        while( self.nodesForAssociation().length>0 )
            self.nodesForAssociation.pop();

        //fill node infos array
        self.selectedNode = self.nodes()[nodeId];
        self.nodeInfos.push({info:'Device id', value:self.selectedNode.id});
        self.nodeInfos.push({info:'Manufacturer', value:self.selectedNode.manufacturer});
        self.nodeInfos.push({info:'Device type', value:self.selectedNode.type});
        self.nodeInfos.push({info:'Product', value:self.selectedNode.product});
        self.nodeInfos.push({info:'Product type', value:self.selectedNode.producttype});
        self.nodeInfos.push({info:'Device version', value:self.selectedNode.version});
        self.nodeInfos.push({info:'Basic device class', value:self.selectedNode.basic});
        self.nodeInfos.push({info:'Generic device class', value:self.selectedNode.generic});
        self.nodeInfos.push({info:'Specific device class', value:self.selectedNode.specific});
        var internalids = '';
        for( i=0; i<self.selectedNode.internalids.length; i++)
        {
            internalids += self.selectedNode.internalids[i] + ', ';
        }
        internalids = internalids.substring(0, internalids.length-2);
        self.nodeInfos.push({info:'Internalids', value:internalids});

        if( self.selectedNode.status )
        {
            self.nodeStatus.push({status:'Query stage', value:self.selectedNode.status.querystage});
            self.nodeStatus.push({status:'Awake', value:(self.selectedNode.status.awake ? 'true' : 'false')});
            self.nodeStatus.push({status:'Listening', value:(self.selectedNode.status.listening ? 'true' : 'false')});
            self.nodeStatus.push({status:'Failed', value:(self.selectedNode.status.failed ? 'true' : 'false')});
        }

        for( i=0; i<self.nodes().length; i++ )
        {
            if( self.nodes()[i].id!=self.selectedNode.id ) //compare string and int
            {
                self.nodesForAssociation.push({key:self.nodes()[i].id, value:self.nodes()[i].type+'('+self.nodes()[i].id+')'});
            }
        }

        //get node parameters
        for( i=0; i<self.selectedNode.params.length; i++ )
        {
            self.nodeParameters.push({
                'currentvalue': ko.observable(self.selectedNode.params[i].currentvalue),
                'help': self.selectedNode.params[i].help,
                'invalid': self.selectedNode.params[i].invalid,
                'items': (self.selectedNode.params[i].items===undefined) ? null : self.selectedNode.params[i].items,
                'label': self.selectedNode.params[i].label,
                'max': self.selectedNode.params[i].max,
                'min': self.selectedNode.params[i].min,
                'readonly': self.selectedNode.params[i].readonly,
                'type': self.selectedNode.params[i].type,
                'units': self.selectedNode.params[i].units,
                'index': self.selectedNode.params[i].index,
                'commandclassid': self.selectedNode.params[i].commandclassid
            });
        }

        //get node associations
        if( self.selectedNode.numgroups )
        {
            for( var group=1; group<=self.selectedNode.numgroups; group++ )
            {
                zwave.getAssociations(self.selectedNode.id, group, function(associations, label, node, group) {
                    //get associated node infos
                    var id, i;
                    var assos = [];
                    var targets = ko.observableArray([]);

                    //fill list of targets with all nodes
                    for( i=0; i<self.nodes().length; i++ )
                    {
                        targets.push({key:self.nodes()[i].id, value:self.nodes()[i].type+'('+self.nodes()[i].id+')'});
                    }

                    //fill associations
                    for( var key in associations )
                    {
                        id = associations[key]-1;
                        if( id<self.nodes().length )
                        {
                            assos.push({asso:self.nodes()[id].type+'('+self.nodes()[id].id+')', node:parseInt(node), group:group, target:parseInt(self.nodes()[id].id), add:false});
                        }
                        //remove associated node from targets
                        for( i=targets().length-1; i>=0; i-- )
                        {
                            if( targets()[i].key==self.nodes()[id].id )
                            {
                                targets().splice(i,1);
                                break;
                            }
                        }
                    }

                    //remove itself from targets
                    for( i=targets().length-1; i>=0; i-- )
                    {
                        if( targets()[i].key==self.selectedNode.id )
                        {
                            targets().splice(i,1);
                            break;
                        }
                    }

                    //append association
                    assos.push({asso:'', node:parseInt(node), group:group, target:ko.observable(), targets:targets, add:true});
                    self.nodeAssociations.push({label:label, assos:assos});
                });
            }
        }
        

        //open modal
        $('#nodeDetails').modal('show');
    };
    
    //get nodes and build nodes graph
    self.init = function() {
        zwave.getNodes(function(nodelist) {
            for( var id in nodelist ) 
            {
                var newNode = nodelist[id];
                newNode.id = id;
                self.nodes.push(newNode);
            }
            self.nodesCount(self.nodes().length);
            if( self.nodes().length>0 )
            {
                zwave.buildChordGraph('#nodesDependency', self.nodes(), self.openNodeDetails);
                //zwave.buildDirectedGraph('#nodesDependency', self.nodes(), self.openNodeDetails);
            }
        });
    };

    //reset controller
    self.reset = function() {
        var msg = $('#reallyreset').html();
        if( confirm(msg) )
        {
            zwave.reset();
        }
    };

    //heal node
    self.healNode = function() {
        zwave.healNode(self.selectedNode.id);
    };

    //add node
    self.addNode = function() {
        zwave.addNode();
    };

    //remove node
    self.removeNode = function() {
        if( self.selectedNode.id )
        {
            var msg = $('#reallyremovenode').html();
            if( confirm(msg) )
            {
                zwave.removeNode(self.selectedNode.id);
            }
        }
        else
        {
            zwave.removeNode();
        }
    };

    //refresh node
    self.refreshNode = function() {
        zwave.refreshNode(self.selectedNode.id);
    };

    //test node
    self.testNode = function() {
        zwave.testNode(self.selectedNode.id);
    };

    //request all configuration parameters from node
    self.requestAllConfigParams = function() {
        zwave.requestAllConfigParams(self.selectedNode.id);
    };

    //set device parameter
    self.setParameter = function(param) {
        zwave.setParameter(self.selectedNode.id, param.commandclassid, param.index, param.currentvalue());
    };

    //create association
    self.createAssociation = function() {
        //console.log("add association node="+self.selectedNode.id+" group="+(self.selectedNode.numgroups+1)+" target="+self.selectedNodeForAssociation().key);
        zwave.addAssociation(self.selectedNode.id, (self.selectedNode.numgroups+1), self.selectedNodeForAssociation().key);
    };

    //add association
    self.addAssociation = function(asso) {
        //console.log("add association node="+asso.node+" group="+asso.group+" target="+asso.target().key);
        zwave.addAssociation(asso.node, asso.group, asso.target().key);
    };

    //remove association
    self.removeAssociation = function(asso) {
        var msg = $('#reallyremoveassociation').html();
        if( confirm(msg) )
        {
            zwave.removeAssociation(asso.node, asso.group, asso.target);
        }
    };

    //test network
    self.testNetwork = function() {
        zwave.testNetwork();
    };

    //download config
    self.downloadConfig = function() {
        zwave.downloadConfig();
    };

    //save config
    self.saveConfig = function() {
        zwave.saveConfig();
    };

    //cancel
    self.cancel = function() {
        zwave.cancel();
    };

    //turn on all nodes
    self.allOn = function() {
        zwave.allOn();
    };

    //turn off all nodes
    self.allOff = function() {
        zwave.allOff();
    };

    //heal network
    self.healNetwork = function() {
        zwave.healNetwork();
    };

    //get statitics
    zwave.getStatistics(function(statistics) {
        for( var key in statistics )
        {
            self.stats.push({'stat':key, 'value':statistics[key]});
        }
    });
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var zwave = new Zwave(agocontrol.devices(), agocontrol);
    var model = new zwaveConfig(zwave);
    return model;
}

function afterrender_template(model)
{
    //template is rendered, we can build graph
    if( model )
    {
        model.init();
    }
}
