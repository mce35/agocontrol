/**
 * Agoblockly plugin
 * @returns {agoblockly}
 */
function agoBlocklyPlugin(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.luaControllerUuid = null;
    self.availableScripts = ko.observableArray([]);
    self.selectedScript = ko.observable('');
    self.scriptName = ko.observable('untitled');
    self.scriptSaved = ko.observable(true);
    self.scriptLoaded = false;
    self.debugEvents = ko.observable();
    self.debugSelectedEvent = ko.observable();
    self.debugEventProps = ko.pureComputed(function() {
        var ev = self.debugSelectedEvent();
        var props = [];
        if( ev && self.agocontrol.schema().events[ev] )
        {
            for( var i=0; i<self.agocontrol.schema().events[ev].parameters.length; i++ )
            {
                props.push({'label':self.agocontrol.schema().events[ev].parameters[i], 'value':ko.observable('')});
            }
        }
        return props;
    });
    self.debugging = false;
    self.defaultWorkspace = '<xml>'
        + '<block type="controls_if">'
        + '<value name="IF0">'
        + '<block type="agocontrol_content">'
        + '<value name="EVENT">'
        + '<block type="agocontrol_eventAll"/>'
        + '</value>'
        + '</block>'
        + '</value>'
        + '</block>'
        + '</xml>';
    self.workspace = null; //blockly workspace
    self.email = '';
    self.phone = '';
    self.contactsUpdated = false;
    self.blockNumber = 3; //number of blocks with default workspace

    //update default contacts
    self.updateDefaultContacts = function()
    {
        var content = {
            uuid: self.luaControllerUuid,
            command: 'getcontacts'
        };
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.email = res.data.email;
                self.phone = res.data.phone;
                self.contactsUpdated = true;
            });
    };

    //luacontroller uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='luacontroller' )
            {
                self.luaControllerUuid = devices[i].uuid;
                self.updateDefaultContacts();
                break;
            }
        }
    }

    //return default contacts
    self.getDefaultContacts = function()
    {
        return {
            'updated': self.contactsUpdated,
            'email': self.email,
            'phone': self.phone
        };
    };

    //get unconnected block
    self.getUnconnectedBlock = function()
    {
        var blocks = self.workspace.getAllBlocks();
        for (var i = 0, block; block = blocks[i]; i++)
        {
            var connections = block.getConnections_(true);
            for (var j = 0, conn; conn = connections[j]; j++)
            {
                if (!conn.sourceBlock_ || (conn.type == Blockly.INPUT_VALUE || conn.type == Blockly.OUTPUT_VALUE) && !conn.targetConnection)
                {
                    return block;
                }
            }
        }
        return null;
    };
    
    //get block with warning
    self.getBlockWithWarning = function()
    {
        var blocks = self.workspace.getAllBlocks();
        for (var i = 0, block; block = blocks[i]; i++)
        {
            if (block.warning)
            {
                return block;
            }
        }
        return null;
    };
    
    //blink specified block
    self.blinkBlock = function(block)
    {
        for(var i=300; i<3000; i=i+300)
        {
            setTimeout(function() { block.select(); },i);
            setTimeout(function() { block.unselect(); },i+150);
        }
    };

    //check blocks
    self.checkBlocks = function(notifySuccess)
    {
        var warningText;
        if( self.workspace.getAllBlocks().length===0 )
        {
            //nothing to save
            notif.info('#nb');
            return;
        }
        var badBlock = self.getUnconnectedBlock();
        if (badBlock)
        {
            warningText = 'This block is not properly connected to other blocks.';
        }
        else
        {
            badBlock = self.getBlockWithWarning();
            if (badBlock)
            {
                warningText = 'Please fix the warning on this block.';
            }
        }

        if (badBlock)
        {
            notif.error(warningText);
            self.blinkBlock(badBlock);
            return false;
        }

        if( notifySuccess )
            notif.success('All blocks seems to be valid');

        return true;
    };

    //merge specified xml and lua code
    self.mergeXmlAndLua = function(xml, lua)
    {
        var out = '-- /!\\ Lua code generated by agoblockly. Do not edit manually.\n';
        out += '--[[\n' + xml + '\n]]\n';
        out += lua;
        return out;
    };

    //split specified script in xml and lua part
    self.unmergeXmlAndLua = function(script)
    {
        var out = {'error':false, 'xml':'', 'lua':''};
        //remove line breaks
        script = script.replace(/(\r\n|\n|\r)/gm,"");
        //extract xml and lua
        var re = /--.*--\[\[(.*)\]\](.*)/;
        var result = re.exec(script);
        if( result.length==3 )
        {
            out['xml'] = result[1];
            out['lua'] = result[2];
        }
        else
        {
            out['error'] = true;
        }
        return out;
    };

    //save script
    self.saveScript = function()
    {
        var scriptName = self.scriptName();
        //replace all whitespaces
        scriptName = scriptName.replace(/\s/g, "_");
        //append blockly_ prefix if necessary
        if( scriptName.indexOf('blockly_')!==0 )
        {
            scriptName = 'blockly_'+scriptName;
        }

        var content = {
            uuid: self.luaControllerUuid,
            command: 'setscript',
            name: scriptName,
            script: self.mergeXmlAndLua(self.getXml(), self.getLua())
        };
        self.agocontrol.sendCommand(content)
            .then(function(res){
                notif.success('#ss');
                self.scriptName(scriptName.replace('blockly_', ''));
                self.scriptSaved(true);
            })
            .catch(function(error){
                notif.fatal('#nr', 0);
            });
    };

    //return xml code of blocks
    self.getXml = function()
    {
        var dom = Blockly.Xml.workspaceToDom(self.workspace);
        return Blockly.Xml.domToText(dom);
    };

    //set blocks structure
    self.setXml = function(xml)
    {
        //clear existing blocks
        self.workspace.clear();
        //load xml
        try {
            var dom = Blockly.Xml.textToDom(xml);
            Blockly.Xml.domToWorkspace(self.workspace, dom);
            self.workspace.render();
        }
        catch(e) {
            //exception
            console.error('Exception during xml loading:'+e);
            return false;
        }
        //check if loaded
        if( self.workspace.getTopBlocks(false).length===0 )
        {
            //no block in workspace
            return false;
        }
        else
        {
            //loaded successfully
            return true;
        }
    };

    //return lua code of blocks
    self.getLua = function()
    {
        return Blockly.Lua.workspaceToCode(self.workspace);
    };

    //set workspace
    self.setWorkspace = function(workspace)
    {
        self.workspace = workspace;
        //add listener
        self.workspace.addChangeListener(self.onWorkspaceChanged);
    };

    //callback when workspace changed
    self.onWorkspaceChanged = function()
    {
        if( self.workspace.rendered )
        {
            //only change button state if number of blocks is different from last time
            var blockNumber = self.workspace.getAllBlocks().length;
            if( blockNumber!=self.blockNumber )
            {
                //update number of blocks
                self.blockNumber = blockNumber;
                if( !self.scriptLoaded )
                {
                    self.scriptSaved(false);
                }
                else if( self.scriptLoaded )
                {
                    self.scriptLoaded = false;
                }
            }
        }
    };

    //load scripts
    self.loadScripts = function(callback)
    {
        //get scripts
        self.agocontrol.block($('#agoGrid'));
        var content = {
            uuid: self.luaControllerUuid,
            command: 'getscriptlist'
        };
        self.agocontrol.sendCommand(content)
            .then(function(res){
                //update ui variables
                self.availableScripts([]);
                for( var i=0; i<res.data.scriptlist.length; i++ )
                {
                    //only keep agoblockly scripts
                    if( res.data.scriptlist[i].indexOf('blockly_')===0 )
                    {
                        self.availableScripts.push({'name':res.data.scriptlist[i].replace('blockly_','')});
                    }
                }

                //callback
                if( callback!==undefined )
                    callback();
            })
            .catch(function(err){
                notif.fatal('#nr');
            })
            .finally(function(){
                self.agocontrol.unblock($('#agoGrid'));
            });
    };

    //load a script
    self.loadScript = function(scriptName, scriptContent)
    {
        //decode (base64) and extract xml from script content
        var script = self.unmergeXmlAndLua(B64.decode(scriptContent));
        if( !script['error'] )
        {
            if( self.setXml(script['xml']) )
            {
                //remove blockly_ prefix
                if( scriptName.indexOf('blockly_')===0 )
                {
                    scriptName = scriptName.replace('blockly_', '');
                }
                self.scriptName(scriptName);
                self.scriptSaved(true);
                self.scriptLoaded = true;
                notif.success('#sl');
            }
            else
            {
                //script corrupted
                notif.error('#sc');
            }
        }
        else
        {
            //script corrupted
            notif.error('#sc');
        }
    };

    //delete a script
    self.deleteScript = function(script, callback)
    {
        var content = {
            uuid: self.luaControllerUuid,
            command: 'delscript',
            name: script
        };
        self.agocontrol.sendCommand(content)
            .then(function(res){
                notif.success('#sd');
                if( callback!==undefined )
                    callback();
            })
            .catch(function(err){
                notif.error('#nd');
            });
    };

    //rename a script
    self.renameScript = function(item, oldScript, newScript)
    {
        var content = {
            uuid: self.luaControllerUuid,
            command: 'renscript',
            oldname: 'blockly_'+oldScript,
            newname: 'blockly_'+newScript
        };
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                item.attr('data-oldname', newScript);
                notif.success('#rss');
                self.loadScripts();
                return true;
            })
            .catch(function(err){
                notif.error('#rsf');
            });
    };

    //Add default blocks
    self.addDefaultBlocks = function()
    {
        var xml = Blockly.Xml.textToDom(self.defaultWorkspace);
        Blockly.Xml.domToWorkspace(self.workspace, xml);
    };

    //============================
    //ui events
    //============================

    //clear button
    self.clear = function()
    {
        var exec = false;

        if( self.workspace.getAllBlocks().length>3 )
        {
            if( window.confirm("Start new script?") )
            {
                exec = true;
            }
        }
        else
        {
            exec = true;
        }

        if( exec )
        {
            self.workspace.clear();
            self.scriptName('untitled');
            self.scriptSaved(true);
            self.addDefaultBlocks();
        }
    };

    //check button
    self.check = function()
    {
        self.checkBlocks(true);
    };

    //save button
    self.save = function()
    {
        //check code
        if( !self.checkBlocks(false) )
        {
            //TODO allow script saving event if there are errors
            return;
        }

        //request script filename if necessary
        if( self.scriptName()==='untitled' )
        {
            $("#saveasDialog").modal('show');
        }
        else
        {
            //script name already specified, save script
            self.saveScript();
        }
    };

    //saveas button
    self.saveas = function()
    {
        //check code
        if( !self.checkBlocks(false) )
        {
            //TODO allow script saving event if there are errors
            return;
        }

        //request script filename if necessary
        $("#saveasDialog").modal('show');
    };

    //save dialog ok button
    self.saveOk = function()
    {
        self.scriptName( $.trim(self.scriptName()) );
        if( self.scriptName().length===0 || self.scriptName()==='untitled' )
        {
            //invalid script name
            notif.warning('#sn');
            self.scriptName('untitled');
        }
        else
        {
            //save script
            self.saveScript();
        }
        $("#saveasDialog").modal('hide');
    };

    //save dialog cancel button
    self.saveCancel = function()
    {
        notif.info('#ns');
        $("#saveasDialog").modal('hide');
    };

    //delete script
    self.delete = function()
    {
        var content = {
            uuid: self.luaControllerUuid,
            command: 'delscript',
            name: 'TODO'
        };
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                // ??? error???
                notif.error('#sd');
            })
            .catch(function(err){
                //unable to delete
                notif.error('#nd');
            });
    };

    //import script
    self.importScript = function()
    {
        //init upload
        $('#fileupload').fileupload({
            dataType: 'json',
            formData: { 
                uuid: self.luaControllerUuid
            },
            done: function (e, data) {
                if( data.result && data.result.result )
                {
                    if( data.result.result.error.len>0 )
                    {
                        notif.error('Unable to import script');
                        console.error('Unable to upload script: '+data.result.result.error);
                    }
                    else
                    {
                        if( data.result.result.count>0 )
                        {
                            $.each(data.result.result.files, function (index, file) {
                                notif.success('Script "'+file.name+'" imported successfully');
                            });
                            self.loadScripts();
                        }
                        else
                        {
                            //no file uploaded
                            notif.error('Script import failed: '+data.result.result.error);
                        }
                    }
                }
                else
                {
                    notif.fatal('Unable to import script: internal error');
                }
            },
            progressall: function (e, data) {
                var progress = parseInt(data.loaded / data.total * 100, 10);
                $('#progress').css('width', progress+'%');
            }
        });

        $("#importDialog").modal('show');
    };

    //load script
    self.load = function()
    {
        self.loadScripts(function()
        {
            //open script dialog
            $("#loadDialog").modal('show');
        });
    };

    //debug event handler
    self.debugEventHandler = function(event)
    {
        if( event.event==='event.system.debugscript' )
        {
            //append debug message
            if( event.type===0 )
            {
                //start message
                $('#debugContainer > ul').append('<li class="list-group-item list-group-item-info">'+event.msg+'</i>');
            }
            else if( event.type===1 )
            {
                //end message
                $('#debugContainer > ul').append('<li class="list-group-item list-group-item-info">'+event.msg+'</i>');
                //stop debugging
                self.stopDebug();
            }
            else if( event.type===2 )
            {
                //error message
                $('#debugContainer > ul').append('<li class="list-group-item list-group-item-danger">'+event.msg+'</i>');
            }
            else if( event.type===3 )
            {
                //default message
                $('#debugContainer > ul').append('<li class="list-group-item">'+event.msg+'</i>');
            }
        }
    };

    //search events in lua
    self.searchEvents = function()
    {
        //init
        var results = [];
        var lua = self.getLua();

        //prepare regexp
        var re = /^.*content\.subject\ ==\ \'(.*)\'.*$/gm; 
        var m;
         
        //search for events
        while( (m = re.exec(lua))!==null )
        {
            if( m.index===re.lastIndex )
            {
                re.lastIndex++;
            }
            results.push(m[1]);
        }

        return results;
    }

    //start debugging
    self.startDebug = function()
    {
        if( self.debugging )
        {
            notif.info('Already debugging');
            return;
        }
        self.debugging = true;

        //add event handler
        self.agocontrol.addEventHandler(self.debugEventHandler);

        //clear debug area
        self.clearDebug();

        //build data
        var data = {};
        data['subject'] = self.debugSelectedEvent();
        for( var i=0; i<self.debugEventProps().length; i++ )
        {
            data[self.debugEventProps()[i].label] = self.debugEventProps()[i].value();
        }

        //launch debug command
        var content = {
            uuid: self.luaControllerUuid,
            command: 'debugscript',
            script: self.getLua(),
            data: data
        };
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //notif.success('Debug started');
            })
            .catch(function(error){
                notif.error(getErrorMessage(error));
            });
    };

    //stop debug
    self.stopDebug = function()
    {
        //remove handler
        self.agocontrol.removeEventHandler(self.debugEventHandler);
        self.debugging = false;
    };

    //clear debug area
    self.clearDebug = function()
    {
        $('#debugContainer > ul').empty();
    };

    //debug script
    self.openDebug = function()
    {
        //get trigger event from script
        var events = self.searchEvents();
        self.debugEvents(events);

        //open dialog
        $("#debugDialog").modal('show');
    };

    //view lua source code
    self.viewlua = function()
    {
        //check code first
        if( !self.checkBlocks(false) )
            return;

        //fill dialog content
        var content = document.getElementById('luaContent');
        var code = self.getLua();
        content.textContent = code;
        if (typeof prettyPrintOne == 'function')
        {
            code = content.innerHTML;
            code = prettyPrintOne(code, 'lang-lua');
            content.innerHTML = code;
        }
        //open dialog
        $('#luaDialog').modal('show')
    };

    //load script
    self.uiLoadScript = function(script)
    {
        var content = {
            uuid: self.luaControllerUuid,
            command: 'getscript',
            name: 'blockly_'+script
        };
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.loadScript(res.data.name, res.data.script);
            });

        $("#loadDialog").modal('hide');
    };

    //rename script
    self.uiRenameScript = function(item, td, tr)
    {
        if( $(td).hasClass('rename_script') )
        {
            $(td).editable(function(value, settings) {
                self.renameScript($(this), $(this).attr('data-oldname'), value);
                return value;
            },
            {
                data : function(value, settings)
                {
                    return value;
                },
                onblur : "cancel"
            }).click();
        }
    };

    self.grid = new ko.agoGrid.viewModel({
        data: self.availableScripts,
        columns: [
            {headerText:'Script', rowText:'name'},
            {headerText:'Actions', rowText:''}
        ],
        rowCallback: self.uiRenameScript,
        rowTemplate: 'rowTemplate'
    });

    //delete script
    self.uiDeleteScript = function(script)
    {
        var msg = $('#cd').html();
        if( confirm(msg) )
        {
            self.deleteScript('blockly_'+script, function() {
                self.loadScripts();
            });
        }
    };

    //export script
    self.uiExportScript = function(script)
    {
        downloadurl = location.protocol + "//" + location.hostname + (location.port && ":" + location.port) + "/download?filename="+script+"&uuid="+self.luaControllerUuid;
        window.open(downloadurl, '_blank');
    };

    //view model
    this.blocklyViewModel = new ko.blockly.viewModel({
        onWorkspaceChanged: self.onWorkspaceChanged,
        addDefaultBlocks: self.addDefaultBlocks,
        setWorkspace: self.setWorkspace
    });
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    ko.blockly = {
        viewModel: function(config) {
            this.onWorkspaceChanged = config.onWorkspaceChanged;
            this.addDefaultBlocks = config.addDefaultBlocks;
            this.setWorkspace = config.setWorkspace;
        }
    };

    ko.bindingHandlers.blockly = {
        update: function(element, viewmodel) {
            var workspace = null;

            //init blockly
            var interval = window.setInterval(function() {
                if( typeof Blockly!='object' )
                {
                    //blockly not loaded yet
                    return;
                }
                if( typeof BlocklyAgocontrol!='object' )
                {
                    //agocontrol object not loaded yet
                    return;
                }
                var extra = model.getDefaultContacts();
                if( extra && extra.updated===false )
                {
                    //default contacts not loaded yet
                    return;
                }
                if( !document.getElementById('blocklyDiv') )
                {
                    //blockly already loaded (should not happen now because blockly has dispose function)
                    return;
                }
                window.clearInterval(interval);

                //inject blockly
                element.innerHTML = "";
                var blocklyArea = document.getElementById('blocklyArea');
                var blocklyDiv = document.getElementById('blocklyDiv');
                workspace = Blockly.inject(blocklyDiv, {
                    path: "configuration/blockly/blockly/",
                    toolbox: document.getElementById('toolbox'),
                    zoom: {
                        controls: true,
                        wheel: false, //keep wheel to scroll to main page not blockly workspace
                        startScale: 1.0,
                        maxScale: 2.0,
                        minScale: 0.5,
                        scaleSpeed: 1.2
                    },
                    grid: {
                        spacing: 20,
                        length: 2,
                        colour: '#ccc',
                        snap: true
                    }
                });

                //hack to make blockly using maximum page space
                //https://developers.google.com/blockly/installation/injecting-resizable
                var onresize = function(e) {
                    var element = blocklyArea;
                    var x = 0;
                    var y = 0;

                    do {
                        x += element.offsetLeft;
                        y += element.offsetTop;
                        element = element.offsetParent;
                    } while (element);

                    blocklyDiv.style.left = x + 'px';
                    blocklyDiv.style.top = y + 'px';
                    blocklyDiv.style.width = (blocklyArea.offsetWidth-50) + 'px'; //reduce of 50px to take in count margin
                    blocklyDiv.style.height = (blocklyArea.offsetHeight-50) + 'px';
                };
                window.addEventListener('resize', onresize, false);
                onresize();

                //set workspace
                viewmodel().setWorkspace(workspace);

                //init agoblockly
                if( BlocklyAgocontrol!==null && BlocklyAgocontrol.init!==undefined )
                {
                    BlocklyAgocontrol.init(agocontrol.schema(), agocontrol.devices(), agocontrol.variables(), extra);
                }
                else
                {
                    notif.error('Unable to configure Blockly! Event builder shouldn\'t work.');
                }

                //init default blocks
                viewmodel().addDefaultBlocks();
            }, 250);

            //clean up
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                if( workspace!==null )
                {
                    workspace.dispose();
                }
            });
        }
    };

    var model = new agoBlocklyPlugin(agocontrol.devices(), agocontrol);
    return model;
}

function reset_template(model)
{
    //delete blockly divs (bug in blockly?)
    $('.blocklyWidgetDiv').hide();
    $('.blocklyTooltipDiv').hide();
    $('.blocklyToolboxDiv').hide();
}
