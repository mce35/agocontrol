/**
 * @info Blocky for agocontrol
 * @info tanguy.bonneau@gmail.com
 * @see block builder: https://blockly-demo.appspot.com/static/apps/blockfactory/index.html
 */

'use strict';

//@see http://stackoverflow.com/a/2548133/3333386
if (typeof String.prototype.endsWith !== 'function') {
    String.prototype.endsWith = function(suffix) {
        return this.indexOf(suffix, this.length - suffix.length) !== -1;
    };
}

//====================================
//AGOCONTROL OBJECT
//====================================
window.BlocklyAgocontrol = {
    schema: {},
    devices: [],
    variables: [],
    eventBlocks: {},

    //init
    init: function(schema, devices, variables) {
        this.schema = schema;
        this.devices = devices;
        //only variable names are useful
        for( var i=0; i<variables.length; i++ )
        {
            if( variables[i].variable )
            {
                this.variables.push(variables[i].variable);
            }
        }
    },

    //shortens event name to not overload block
    shortensEventName: function(event) {
        if( event.indexOf("system.")!==-1 )
        { 
            //system event, drop it
            return "";
        }
        else
        {
            var out = event.replace("event.", "");
            out = out.replace("environment", "env");
            out = out.replace("device", "dev");
            out = out.replace("mediaplayer", "mplay");
            out = out.replace("proximity", "prox");
            out = out.replace("telecom", "tel");
            out = out.replace("security", "sec");
            out = out.replace("monitoring", "moni");
            return out;
        }
    },

    //string is ending with specified string ?
    endsWith: function(str) {
        return this.indexOf(str, this.length - str.length)!==-1;
    },

    //get device uuids
    getDeviceUuids: function(deviceType) {
        var devices = [];
        var device;
        for( var i=0; i<this.devices.length; i++ )
        {
            device = this.devices[i];
            if( (device.name.length>0 || (device.name.length===0 && device.internalid.endsWith('controller'))) && device.devicetype==deviceType )
            {
                if( device.name.length>0 )
                {
                    devices.push([device.name, device.uuid]);
                }
                else
                {
                    devices.push([device.internalid, device.uuid]);
                }
            }
        }
        //prevent from js crash
        if( devices.length===0 )
            devices.push(['', '']);
        return devices;
    },

    //get device internalids
    getDeviceInternalids: function(deviceType) {
        var devices = [];
        var device;
        for( var i=0; i<this.devices.length; i++ )
        {
            device = this.devices[i];
            if( (device.name.length>0 || (device.name.length===0 && device.internalid.endsWith('controller'))) && device.devicetype==deviceType )
            {
                if( device.name.length>0 )
                {
                    devices.push([device.name, device.internalid]);
                }
                else
                {
                    devices.push([device.internalid, device.internalid]);
                }
            }
        }
        //prevent from js crash
        if( devices.length===0 )
            devices.push(['', '']);
        return devices;
    },

    //get device types
    getDeviceTypes: function() {
        var types = [];
        var duplicates = [];
        var device;
        for( var i=0; i<this.devices.length; i++ )
        {
            device = this.devices[i];
            if( (device.name.length>0 || (device.name.length===0 && device.internalid.endsWith('controller'))) && duplicates.indexOf(device.devicetype)===-1 )
            {
                types.push([device.devicetype, device.devicetype]);
                duplicates.push(device.devicetype);
            }
        }
        //prevent from js crash
        if( types.length===0 )
        {
            types.push(['', '']);
        }
        return types;
    },

    //get device names
    getDeviceNames: function(deviceType) {
        var names = [];
        var duplicates = [];
        var device = null;
        for( var i=0; i<this.devices.length; i++ )
        {
            device = this.devices[i];
            if( (device.name.length>0 || (device.name.length===0 && device.internalid.endsWith('controller'))) && device.devicetype===deviceType && duplicates.indexOf(device.name)===-1 )
            {
                if( device.name.length>0 )
                {
                    names.push([device.name, device.name]);
                    duplicates.push(device.name);
                }
                else
                {
                    names.push([device.internalid, device.name]);
                    duplicates.push(device.internalid);
                }
            }
        }
        //prevent from js crash
        if( names.length===0 )
        {
            names.push(['', '']);
        }
        return names;
    },

    //get all events
    getAllEvents: function() {
        var events = [];
        for( var event in this.schema.events )
        {
            if( event!==undefined )
            {
                var evtName = this.shortensEventName(event);
                if( evtName.length>0 )
                {
                    events.push([evtName, event]);
                }
            }
        }
        //prevent from js crash
        if( events.length===0 )
            events.push(['', '']);
        return events;
    },

    //get device events
    getDeviceEvents: function(deviceType) {
        var events = [];
        if( this.schema.devicetypes[deviceType]!==undefined && this.schema.devicetypes[deviceType].events!==undefined )
        {
            for( var i=0; i<this.schema.devicetypes[deviceType].events.length; i++ )
            {
                var evtName = this.shortensEventName(this.schema.devicetypes[deviceType].events[i]);
                if( evtName.length>0 )
                {
                    events.push([evtName, this.schema.devicetypes[deviceType].events[i]]);
                }
            }
        }
        //prevent from js crash
        if( events.length===0 )
            events.push(['', '']);
        return events;
    },

    //get event properties
    getEventProperties: function(event) {
        var props = [];
        if( this.schema.events[event]!==undefined && this.schema.events[event].parameters )
        {
            for( var i=0; i<this.schema.events[event].parameters.length; i++ )
            {
                props.push([this.schema.events[event].parameters[i], this.schema.events[event].parameters[i]]);
            }
        }
        //prevent from js crash
        if( props.length===0 )
            props.push(['', '']);
        return props;
    },

    //get device properties
    getDeviceProperties: function(deviceType, deviceUuid) {
        var props = {};
        var statePropFound = false;
        if( this.schema.devicetypes[deviceType]!==undefined && this.schema.devicetypes[deviceType].properties!==undefined )
        {
            var prop;
            for( var i=0; i<this.schema.devicetypes[deviceType].properties.length; i++ )
            {
                prop = this.schema.devicetypes[deviceType].properties[i];
                if( this.schema.values[prop]!==undefined )
                {
                    if( this.schema.values[prop].type!==undefined && this.schema.values[prop].name!==undefined )
                    {
                        var content = {};
                        for( var item in  this.schema.values[prop] )
                        {
                            if( item!==undefined )
                                content[item] = this.schema.values[prop][item];
                        }
                        props[prop] = content;
                        if( prop==='state' )
                            statePropFound = true;
                    }
                }
            }
        }
        
        if( deviceUuid )
        {
            //searching for device object
            var device = null;
            for( var i=0; i<this.devices.length; i++)
            {
                if( this.devices[i].uuid===deviceUuid )
                {
                    device = this.devices[i];
                    break;
                }
            }

            //append properties in values array
            if( device!==null && goog.isFunction(device.valueList) )
            {
                var found = false;
                var name;
                for ( var k=0; k<device.valueList().length; k++)
                {
                    found = false;
                    name = device.valueList()[k].name.toLowerCase();
                    for( var prop in props )
                    {
                        if( prop.toLowerCase()===name )
                        {
                            found = true;
                            break;
                        }
                    }
                    if( !found )
                    {
                        props[name] = {'name':name, 'description':name+' value', 'type':'dynamic'};
                    }
                }
            }
        }
        
        //append default state property
        if( !statePropFound )
            props['state'] = {'name':'state', 'description':'device state', 'type':'integer'};
        return props;
    },

    //get device commands
    getDeviceCommands: function(deviceType) {
        var cmds = {};
        if( this.schema.devicetypes[deviceType]!==undefined && this.schema.devicetypes[deviceType].commands!==undefined )
        {
            var cmd;
            for( var i=0; i<this.schema.devicetypes[deviceType].commands.length; i++ )
            {
                cmd = this.schema.devicetypes[deviceType].commands[i];
                if( this.schema.commands[cmd]!==undefined )
                {
                    if( this.schema.commands[cmd].name!==undefined )
                    {
                        var content = {};
                        content.id = cmd;
                        for( var item in this.schema.commands[cmd] )
                        {
                            if( item!==undefined )
                                content[item] = this.schema.commands[cmd][item];
                        }
                        cmds[cmd] = content;
                    }
                }
            }
        }
        return cmds;
    },
    
    //get all values
    getAllValues: function(onlyOption) {
        var values = [];
        for( var value in this.schema.values )
        {
            if( this.schema.values[value].name!==undefined )
            {
                if( onlyOption )
                {
                    if( this.schema.values[value].type!==undefined && this.schema.values[value].type==='option' )
                        values.push([this.schema.values[value].name, value]);
                }
                else
                    values.push([this.schema.values[value].name, value]);
            }
        }
        //prevent from js crash
        if( values.length===0 )
            values.push(['', '']);
        return values;
    },

    //get value
    getValue: function(valueName) {
        var output = {name:null, type:null, options:null};
        if( this.schema.values[valueName]!==undefined && this.schema.values[valueName].name!==undefined )
        {
            output.name = this.schema.values[valueName]['name'];
            if( this.schema.values[valueName].type!==undefined )
            {
                output.type = this.schema.values[valueName].type;
            }
            if( this.schema.values[valueName].options!==undefined )
            {
                output.options = this.schema.values[valueName].options;
            }
        }
        return output;
    },
    
    //get event
    getEvent: function(eventName) {
        var output = {name:null, shortName:null, properties:[]};
        if( eventName.length>0 )
        {
            output.name = eventName;
            var shortName = this.shortensEventName(eventName);
            if( shortName.length===0 )
            {
                output.shortName = eventName;
            }
            else
            {
                output.shortName = shortName;
            }
            output.properties = this.getEventProperties(eventName);
        }
        return output;
    },
       
    //get variables
    getVariables: function() {
        var variables = [];
        for( var i=0; i<this.variables.length; i++ )
        {
            variables.push([this.variables[i], this.variables[i]]);
        }
        if( variables.length===0 )
        {
            variables.push(['', '']);
        }
        return variables;
    },
    
    //clear blockly container
    clearAllBlocks: function(container) {
        if( container===undefined )
            return;
        if( container.fieldRow===undefined )
            console.log('Warning: specified container is not an Input. Unable to clear blocks within.');
        //hack to remove all fields from container (code extracted from core/input.js)
        var field;
        for( var i=container.fieldRow.length-1; i>=0; i--)
        {
            field = container.fieldRow[i];
            field.dispose();
            container.fieldRow.splice(i, 1);
        }
        if (container.sourceBlock_.rendered)
        {
            container.sourceBlock_.render();
            // Removing a field will cause the block to change shape.
            container.sourceBlock_.bumpNeighbours_();
        }
    },
    
    //return all event blocks in group specified block belongs to
    //@return {master, blocks}: 
    //  - master: first event block attached to agocontrol_content that's supposed to "control" all other events
    //  - blocks: array of event blocks in content group (excluding master)
    getEventBlocksInBlockGroup: function(block) {
        //init
        var output = {'master':null, 'blocks':[]};
        var parent = block.getParent();
        var blockLevel = 0;
        
        //looking for main block parent
        while( parent )
        {
            if( parent.getParent() && parent.getSurroundParent() && parent.getParent().id==parent.getSurroundParent().id )
            {
                parent = parent.getParent();
                blockLevel++;
            }
            else
                break;
        }
  
        if( parent )
        {
            var level = -1;
            //walk though parent searching for agocontrol_content block
            for( var i=0; i<parent.getChildren().length; i++ )
            {
                if( parent.getChildren()[i].getSurroundParent() && parent.getChildren()[i].getSurroundParent().id==parent.id )
                {
                    output = this._getEventBlocksInBlockGroupDeep(block, blockLevel, parent.getChildren()[i], 0, output);
                }
            }
        }
        
        return output;
    },
    _getEventBlocksInBlockGroupDeep: function(block, blockLevel, child, childLevel, output) {
        childLevel++;
        if( child.type=="agocontrol_content" && output.master===null )
        {
            //update master event block
            for( var i=0; i<child.getChildren().length; i++ )
            {
                if( child.getChildren()[i].type=="agocontrol_eventAll" || child.getChildren()[i].type=="agocontrol_deviceEvent" )
                {
                    output.master = child.getChildren()[i];
                }
            }
        }
        else if( child.type==="agocontrol_contentProperty" )
        {
            //update blocks array
            if( output.master && output.master.id!==child.id )
            {
                output.blocks.push(child);
            }
        }
        
        for( var i=0; i<child.getChildren().length; i++ )
        {
            output = this._getEventBlocksInBlockGroupDeep(block, blockLevel, child.getChildren()[i], childLevel, output);
        }
        return output;
    },
    
    //update event blocks
    updateEventBlocks: function(block) {
        //get all blocks in block group
        var blocks = this.getEventBlocksInBlockGroup(block);
        
        //hack for optimization
        if( this.eventBlocks[block] &&
            ( (this.eventBlocks[block].master && blocks.master && this.eventBlocks[block].master.id===blocks.master.id) ||
            (!this.eventBlocks[block].master && !blocks.master) ) &&
            (this.eventBlocks[block].blocks.length===blocks.blocks.length) )
        {
            return;
        }

        //reset block inContent flag
        for( var i=0; i<blocks.blocks.length; i++ )
        {
            blocks.blocks[i].inContent = false;
        }
       
        if( blocks.master!==null )
        {
            //enable content stuff
            var event = blocks.master.getEvent();
            for( var i=0; i<blocks.blocks.length; i++ )
            {
                blocks.blocks[i].inContent = true;
                if( blocks.blocks[i].getEvent()!==event )
                {
                    blocks.blocks[i].setEvent(event);
                }
            }
        }

        this.eventBlocks[block] = blocks;
    }
};

//====================================
//ADDITIONAL CORE FUNCTIONS
//====================================

Blockly.FieldTextInput.emailValidator = function(email) {
    var re = /^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    return re.test(email) ? email : null;
};

Blockly.FieldTextInput.phoneNumberValidator = function(phonenumber) {
    var re = /^[a-z]{2}\*[0-9\s]*$/i;
    if( re.test(phonenumber) )
    {
        var parts = phonenumber.split("*");
        var res = phoneNumberParser(parts[1], parts[0]);
        if( res["result"]===false )
            return null;
        else
            return res["phone"];
    }
    else
        return null;
};

//====================================
//AGOCONTROL BLOCKS
//====================================

goog.provide('Blockly.Blocks.agocontrol');
goog.require('Blockly.Blocks');

//no device
Blockly.Blocks['agocontrol_deviceNo'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(20);
        this.appendDummyInput()
            .appendField("No device");
        this.setOutput(true, "String");
        this.setTooltip('Return empty device uuid');
    }
};

//device name
Blockly.Blocks['agocontrol_deviceName'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.lastType = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(20);
        this.container = this.appendDummyInput()
            .appendField("device name of uuid");
        this.appendValueInput("UUID")
            .setCheck(["String", "Device"]);
        this.setInputsInline(true);
        this.setOutput(true, "String");
        this.setTooltip('Return device name');
    }
};

//device uuid block
Blockly.Blocks['agocontrol_deviceUuid'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.lastType = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(20);
        this.container = this.appendDummyInput()
            .appendField("uuid")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getDeviceTypes()), "TYPE")
            .appendField(new Blockly.FieldDropdown([['','']]), "DEVICE");
        this.setInputsInline(true);
        this.setOutput(true, "String");
        this.setTooltip('Return device uuid');
    },

    onchange: function() {
        if( !this.workspace )
            return;
        var currentType = this.getFieldValue("TYPE");
        var currentDevice = null;
        if( this.firstRun )
        {
            currentDevice = this.getFieldValue("DEVICE");
        }
        if( this.lastType!=currentType )
        {
            this.lastType = currentType;
            var devices = window.BlocklyAgocontrol.getDeviceUuids(currentType);
            if( devices.length===0 )
                devices.push(['','']);
            this.container.removeField("DEVICE");
            this.container.appendField(new Blockly.FieldDropdown(devices), "DEVICE");
            if( currentDevice )
            {
                var field = this.getField_("DEVICE");
                if( field )
                {
                    field.setValue(currentDevice);
                }
            }
        }
        
        this.firstRun = false;
    }
};

//device internalid block
Blockly.Blocks['agocontrol_deviceInternalid'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.lastType = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(20);
        this.container = this.appendDummyInput()
            .appendField("internalid")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getDeviceTypes()), "TYPE")
            .appendField(new Blockly.FieldDropdown([['','']]), "DEVICE");
        this.setInputsInline(true);
        this.setOutput(true, "String");
        this.setTooltip('Return device internalid');
    },

    onchange: function() {
        if( !this.workspace )
            return;
        var currentType = this.getFieldValue("TYPE");
        var currentDevice = null;
        if( this.firstRun )
        {
            currentDevice = this.getFieldValue("DEVICE");
        }
        if( this.lastType!=currentType )
        {
            this.lastType = currentType;
            var devices = window.BlocklyAgocontrol.getDeviceInternalids(currentType);
            if( devices.length===0 )
                devices.push(['','']);
            this.container.removeField("DEVICE");
            this.container.appendField(new Blockly.FieldDropdown(devices), "DEVICE");
            if( currentDevice )
            {
                var field = this.getField_("DEVICE");
                if( field )
                {
                    field.setValue(currentDevice);
                }
            }
        }
        
        this.firstRun = false;
    }
};

//no event
Blockly.Blocks['agocontrol_eventNo'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(65);
        this.appendDummyInput()
            .appendField("No event");
        this.setOutput(true, "Event");
        this.setTooltip('Return empty event name');
    }
};

//device event block
Blockly.Blocks['agocontrol_deviceEvent'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.lastType = undefined;
        this.lastDevice = undefined;
        
        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(65);
        this.container = this.appendDummyInput()
            .appendField("event")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getDeviceTypes()), "TYPE")
            .appendField(new Blockly.FieldDropdown([['','']]), "DEVICE")
            .appendField("", "SEP")
            .appendField(new Blockly.FieldDropdown([['','']]), "EVENT");
        this.setInputsInline(true);
        this.setOutput(true, "Event");
        this.setTooltip('Return event name');
    },

    onchange: function() {
        if( !this.workspace )
            return;
            
        //update event blocks
        window.BlocklyAgocontrol.updateEventBlocks(this);
        
        var currentType = this.getFieldValue("TYPE");
        var currentDevice = null;
        var currentEvent = null;
        if( this.firstRun )
        {
            currentDevice = this.getFieldValue("DEVICE");
            currentEvent = this.getFieldValue("EVENT");
        }
        if( this.lastType!=currentType )
        {
            this.lastType = currentType;
            var devices = window.BlocklyAgocontrol.getDeviceUuids(currentType);
            if( devices.length===0 )
                devices.push(['','']);
            this.container.removeField("DEVICE");
            this.container.appendField(new Blockly.FieldDropdown(devices), "DEVICE");
            if( currentDevice )
            {
                var field = this.getField_("DEVICE");
                if( field )
                {
                    field.setValue(currentDevice);
                }
            }

            var events = [];
            if( currentType.length>0 )
            {
                events = window.BlocklyAgocontrol.getDeviceEvents(currentType);
                if( events.length===0 )
                    events.push(['','']);
            }
            else
            {
                events.push(['','']);
            }
            this.container.removeField("SEP");
            this.container.removeField("EVENT");
            this.container.appendField(" ", "SEP");
            this.container.appendField(new Blockly.FieldDropdown(events), "EVENT");
            if( currentEvent )
            {
                var field = this.getField_("EVENT");
                if( field )
                {
                    field.setValue(currentEvent);
                }
            }
        }
        var currentDevice = this.getFieldValue("DEVICE");
        if( this.lastDevice!=currentDevice )
        {
            this.lastDevice = currentDevice;
        }
        
        this.firstRun = false;
    },
    
    //return current event
    getEvent: function() {
        return this.getFieldValue("EVENT");
    },
    
    //set event
    setEvent: function(newEvent) {
        var field = this.getField_("EVENT");
        if( field )
        {
            field.setValue(newEvent);
        }
    }
};

//all events block
Blockly.Blocks['agocontrol_eventAll'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(65);
        this.container = this.appendDummyInput()
            .appendField("event")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getAllEvents()), "EVENT");
        this.setInputsInline(true);
        this.setOutput(true, "Event");
        this.setTooltip('Return event name');
    },
    
    //return current event
    getEvent: function() {
        return this.getFieldValue("EVENT");
    },
    
    //set event
    setEvent: function(newEvent) {
        var field = this.getField_("EVENT");
        if( field )
        {
            return field.setValue(newEvent);
        }
    },
    
    onchange: function() {
        if( this.workspace==null )
            return;
        
        //update event blocks
        window.BlocklyAgocontrol.updateEventBlocks(this);
    }
};

//device property block
Blockly.Blocks['agocontrol_deviceProperty'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.lastType = undefined;
        this.lastDevice = undefined;
        this.properties = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(140);
        this.container = this.appendDummyInput()
            .appendField("device property value")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getDeviceTypes()), "TYPE")
            .appendField(new Blockly.FieldDropdown([['','']]), "DEVICE")
            .appendField(" ", "SEP")
            .appendField(new Blockly.FieldDropdown([['','']]), "PROP");
        this.setInputsInline(true);
        this.setOutput(true, "String");
        this.setTooltip('Return device property value');
    },

    //onchange event
    onchange: function() {
        if( !this.workspace )
            return;

        //update properties list
        var currentType = this.getFieldValue("TYPE");
        var currentDevice = null;
        var currentProp = null;
        if( this.firstRun )
        {
            currentDevice = this.getFieldValue("DEVICE");
            currentProp = this.getFieldValue("PROP");
        }
        if( this.lastType!=currentType )
        {
            this.lastType = currentType;
            var devices = window.BlocklyAgocontrol.getDeviceUuids(currentType);
            if( devices.length===0 )
                devices.push(['','']);
            this.container.removeField("DEVICE");
            this.container.appendField(new Blockly.FieldDropdown(devices), "DEVICE");
            if( currentDevice )
            {
                var field = this.getField_("DEVICE");
                if( field )
                {
                    field.setValue(currentDevice);
                }
            }
        }

        currentDevice = this.getFieldValue("DEVICE");
        if( this.lastDevice!=currentDevice )
        {
            this.lastDevice = currentDevice;
            var props = [];
            if( currentDevice.length>0 )
            {
                this.properties = window.BlocklyAgocontrol.getDeviceProperties(currentType, currentDevice);
                for( var prop in this.properties )
                {
                    props.push([this.properties[prop].name, prop]);
                }
                if( props.length===0 )
                    props.push(['','']);
            }
            else
            {
                props.push(['','']);
            }
            this.container.removeField("SEP");
            this.container.removeField("PROP");
            this.container.appendField(" ", "SEP");
            this.container.appendField(new Blockly.FieldDropdown(props), "PROP");
            if( currentProp )
            {
                var field = this.getField_("PROP");
                if( field )
                {
                    field.setValue(currentProp);
                }
            }
        }
        
        this.firstRun = false;
    }
};

//event property block
Blockly.Blocks['agocontrol_eventProperty'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.properties = undefined;
        this.lastEvent = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(140);
        this.container = this.appendDummyInput()
            .appendField("event prop")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getAllEvents()), "EVENT")
            .appendField(new Blockly.FieldDropdown([['','']]), "PROP");
        this.setInputsInline(true);
        this.setOutput(true, "String");
        this.setTooltip('Return event property name');
    },
  
    //force event selected in dropdown
    setEvent: function(newEvent) {
        var field = this.getField_("EVENT");
        if( field )
        {
            return field.setValue(newEvent);
        }
    },
  
    //return current event
    getEvent: function() {
        return this.getFieldValue("EVENT");
    },

    //onchange event
    onchange: function() {
        if( !this.workspace )
            return;
            
        //update event blocks
        window.BlocklyAgocontrol.updateEventBlocks(this);
        
        //check if block is nested in "content" block
        /*if( !this.inContent )
            this.setWarningText('Property event block MUST be nested in a "content" block (agocontrol->logic->triggered event is)');
        else
            this.setWarningText(null);*/
            
        //update properties list
        var currentEvent = this.getFieldValue("EVENT");
        var currentProp = null;
        if( this.firstRun )
        {
            currentProp = this.getFieldValue("PROP");
        }
        if( this.lastEvent!=currentEvent )
        {
            this.lastEvent = currentEvent;
            var events = window.BlocklyAgocontrol.getEventProperties(currentEvent);
            if( events.length===0 )
                events.push(['','']);
            this.container.removeField("PROP");
            this.container.appendField(new Blockly.FieldDropdown(events), "PROP");
            
            //select property value if necessary
            if( currentProp )
            {
                var field = this.getField_("PROP");
                if( field )
                {
                    field.setValue(currentProp);
                }
            }
        }
        
        this.firstRun = false;
    }
};

//device command
Blockly.Blocks['agocontrol_sendMessage'] = {
    init: function() {
        //members
        this.commands = undefined;
        this.lastType = undefined;
        this.lastDevice = undefined;
        this.lastCommand = undefined;
        this.customFields = [];
        this.customBlocks = [];

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(290);
        this.appendDummyInput()
            .appendField("sendMessage");
        this.container = this.appendDummyInput()
            .appendField("to device")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getDeviceTypes()), "TYPE")
            .appendField(new Blockly.FieldDropdown([['','']]), "DEVICE");
        this.containerCommand = this.appendDummyInput()
            .appendField("command")
            .setAlign(Blockly.ALIGN_RIGHT)
            .appendField(new Blockly.FieldDropdown([['','']]), "COMMAND");
        this.setPreviousStatement(true, "null");
        this.setNextStatement(true, "null");
        this.setTooltip('Send a message to execute a command on agocontrol');
    },
  
    //generate output xml according to block content
    mutationToDom: function() {
        var container = document.createElement('mutation');
        container.setAttribute('currenttype', this.lastType);
        container.setAttribute('currentdevice', this.lastDevice);
        container.setAttribute('currentcommand', this.lastCommand);
        container.setAttribute('duplicated', ((this.customBlocks.length===0) ? false : true));
        return container;
    },
  
    //generate block content according to input xml
    domToMutation: function(xmlElement) {
        var currentType = xmlElement.getAttribute('currenttype');
        var currentDevice = xmlElement.getAttribute('currentdevice');
        var currentCommand = xmlElement.getAttribute('currentcommand');
        var duplicated = xmlElement.getAttribute('duplicated');
        this._generateContent(currentType, currentDevice, currentCommand, duplicated);
    },
  
    //get fields name
    getFields: function() {
        var fields = {};
        for( var i=0; i<this.customFields.length; i++)
        {
            fields["CUSTOM_"+this.customFields[i]] = this.customFields[i];
        }
        return fields;
    },

    //add custom field (internal use)
    _addCustomField: function(key, desc, checkType, block, duplicated) {
        //duplicate operation creates dependent blocks by itself, so no need to create them twice
        if( !duplicated )
        {
            block.initSvg();
        }
        else
        {
            block.dispose();
        }

        //create custom field
        var input = this.appendValueInput("CUSTOM_"+key)
                    .setAlign(Blockly.ALIGN_RIGHT)
                    .setCheck(checkType);
        this.customFields.push(key);
        if( desc!==undefined && desc.length>0 )
        {
            input.appendField('- '+desc);
        }
        else
        {
            //no description in schema.yaml, use name instead
            input.appendField('- '+key);
        }
        if( !duplicated )
        {
            block.outputConnection.connect(input.connection);
            block.render();
            this.customBlocks.push(block);
        }
    },

    //clear custom fields (internal use);
    _clearCustomFields: function() {
        for( var i=0; i<this.customFields.length; i++)
        {
            //get input
            var input = this.getInput("CUSTOM_"+this.customFields[i]);
            if( input!==undefined )
            {
                if( input.connection!==null )
                {
                    input.connection.targetConnection.sourceBlock_.dispose();
                }
                this.removeInput("CUSTOM_"+this.customFields[i]);
            }
        }
        while( this.customFields.length>0 )
        {
            this.customFields.pop();
        }
        while( this.customBlocks.length>0 )
        {
            var block = this.customBlocks.pop();
            block.dispose();
        }
    },
  
    //generate content
    _generateContent: function(currentType, currentDevice, currentCommand, duplicated) {
        //update devices list
        if( !currentType )
            currentType = this.getFieldValue("TYPE");
        if( this.lastType!=currentType )
        {
            this.lastType = currentType;
            var devices = window.BlocklyAgocontrol.getDeviceUuids(currentType);
            if( devices.length===0 )
                devices.push(['','']);
            this.container.removeField("DEVICE");
            this.container.appendField(new Blockly.FieldDropdown(devices), "DEVICE");
        }
        
        //update commands list
        if( !currentDevice )
            currentDevice = this.getFieldValue("DEVICE");
        if( this.lastDevice!=currentDevice )
        {
            this.lastDevice = currentDevice;
            var cmds = [];
            if( currentDevice.length>0 )
            {
                this.commands = window.BlocklyAgocontrol.getDeviceCommands(currentType);
                for( var cmd in this.commands )
                {
                    cmds.push([this.commands[cmd].name, this.commands[cmd].id]);
                }
                if( cmds.length===0 )
                    cmds.push(['','']);
            }
            else
            {
                cmds.push(['','']);
            }
            window.BlocklyAgocontrol.clearAllBlocks(this.containerCommand);
            this.containerCommand.appendField("command");
            this.containerCommand.appendField(new Blockly.FieldDropdown(cmds), "COMMAND");
        }

        //update block content according to selected type
        if( !currentCommand )
            currentCommand = this.getFieldValue("COMMAND");
        if( this.lastCommand!=currentCommand )
        {
            this.lastCommand = currentCommand;
            this._clearCustomFields();
            if( currentCommand.length>0 && this.commands[currentCommand]!==undefined && this.commands[currentCommand].parameters!==undefined )
            {
                var newBlock = null;
                var checkType = null;
                for( var param in this.commands[currentCommand].parameters )
                {
                    if( this.commands[currentCommand].parameters[param].type!==undefined )
                    {
                        switch( this.commands[currentCommand].parameters[param].type )
                        {
                            case "integer":
                            case "number":
                                checkType = "Number";
                                newBlock = Blockly.Block.obtain(this.workspace, 'math_number');
                                break;
                            case "string":
                                checkType = "String";
                                newBlock = Blockly.Block.obtain(this.workspace, 'text');
                                break;
                            case "boolean":
                                checkType = "Boolean";
                                newBlock = Blockly.Block.obtain(this.workspace, 'logic_boolean'); 
                                break;
                            case "option":
                                checkType = "String";
                                newBlock = Blockly.Block.obtain(this.workspace, 'agocontrol_fixedItemsList');
                                var opts = [];
                                if( this.commands[currentCommand].parameters[param].options!==undefined )
                                {
                                    for( var i=0; i<this.commands[currentCommand].parameters[param].options.length; i++)
                                    {
                                        opts.push([this.commands[currentCommand].parameters[param].options[i], this.commands[currentCommand].parameters[param].options[i]]);
                                    }
                                }
                                newBlock.setItems(opts);
                                break;
                            case "email":
                                checkType = "Email";
                                newBlock = Blockly.Block.obtain(this.workspace, 'agocontrol_email');
                                break;
                            case "color":
                            case "colour":
                                checkType = "Colour";
                                newBlock = Blockly.Block.obtain(this.workspace, 'colour_picker');
                                break;
                            case "phone":
                                checkType = "Phone";
                                newBlock = Blockly.Block.obtain(this.workspace, 'agocontrol_phoneNumber');
                                break;
                            case "uuid":
                                checkType = "Device";
                                newBlock = Blockly.Block.obtain(this.workspace, 'agocontrol_device');
                                break;
                            default:
                                //allow any type
                                checkType = "String";
                                newBlock = Blockly.Block.obtain(this.workspace, 'text');
                                break;
                        }
                    }
                    else
                    {
                        //unknown block type, allow any type
                        checkType = "String";
                        newBlock = Blockly.Block.obtain(this.workspace, 'text');
                    }
                    this._addCustomField(param, this.commands[currentCommand].parameters[param].name, checkType, newBlock, duplicated);
                }
            }
        }
    },

    //onchange event
    onchange: function() {
        if( !this.workspace )
            return;

        //update commands list
        this._generateContent(null, null, null, false);
    }
};

//get variable block
Blockly.Blocks['agocontrol_getVariable'] = {
    init: function() {
        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(330);
        this.appendDummyInput()
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getVariables()), "VARIABLE");
        this.setOutput(true, "String");
        this.setTooltip('Return agocontrol variable value (string format!)');
    },
    
    //return selected variable name
    getVariable: function() {
        return this.getFieldValue("VARIABLE") || '';
    }
};

//set variable block
Blockly.Blocks['agocontrol_setVariable'] = {
    init: function() {
        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(330);
        this.appendValueInput("VALUE")
            .setCheck("String")
            .appendField("set")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getVariables()), "VARIABLE")
            .appendField("to");
        this.setInputsInline(true);
        this.setPreviousStatement(true, "null");
        this.setNextStatement(true, "null");
        this.setTooltip('Set agocontrol variable value');
    },
    
    //return selected variable name
    getVariable: function() {
        return this.getFieldValue("VARIABLE") || '';
    }
};

//list of fixed items
Blockly.Blocks['agocontrol_fixedItemsList'] = {
    init: function() {
        //members
        this.items = [['','']]; //empty list
        
        //block definition
        //this.setHelpUrl("TODO");
        this.setColour(160);
        this.setOutput(true, 'String');
        this.container = this.appendDummyInput()
            .appendField(new Blockly.FieldDropdown(this.items), 'LIST');
        this.setTooltip("Return selected list item");
    },
  
    //set list items
    //items must be list [['key','val'], ...]
    setItems: function(items) {
        //regenerate list
        this.container.removeField("LIST");
        //prevent from js crash
        if( items.length===0 )
            items = [['','']];
        this.container.appendField(new Blockly.FieldDropdown(items), "LIST");
    },
  
    //return selected item
    getSelectedItem: function() {
        return this.getFieldValue("LIST") || '';
    }
};

//email block
Blockly.Blocks['agocontrol_email'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(160);
        this.appendDummyInput()
            .appendField(new Blockly.FieldTextInput('john@smith.com', Blockly.FieldTextInput.emailValidator), 'EMAIL');
        this.setOutput(true, 'Email');
        this.setTooltip("An email");
    }
};

//phone number block
Blockly.Blocks['agocontrol_phoneNumber'] = {
    init: function() {
        this.setHelpUrl('http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2#Officially_assigned_code_elements');
        this.setColour(160);
        this.appendDummyInput()
            .appendField(new Blockly.FieldTextInput('us*562 555 5555', Blockly.FieldTextInput.phoneNumberValidator), 'PHONE');
        this.setOutput(true, 'Phone');
        this.setTooltip("A phone number <Alpha-2 code>*<real phone number>");
    }
};

//sleep function
Blockly.Blocks['agocontrol_sleep'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(290);
        this.appendValueInput("DURATION")
            .setCheck("Number")
            .appendField("sleep during");
        this.appendDummyInput()
            .appendField("seconds");
        this.setInputsInline(true);
        this.setPreviousStatement(true, "null");
        this.setNextStatement(true, "null");
        this.setTooltip('Sleep during specified amount of seconds (be carefull it will defer other script!)');
    }
};

//print event content to output
Blockly.Blocks['agocontrol_printContent'] = {
    init: function() {
        this.setColour(160);
        this.appendDummyInput()
            .appendField("Print event content");
        this.setPreviousStatement(true);
        this.setNextStatement(true);
        this.setTooltip('Print event content');
    }
};

//content block
Blockly.Blocks['agocontrol_content'] = {
    init: function() {
        //members
        this.currentEvent = null;
        this.recurseCounter = 0;
        
        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(210);
        this.appendValueInput("EVENT")
            .setCheck("Event")
            .appendField("triggered event is");
        this.setOutput(true, "Boolean");
        this.setTooltip('Condition on triggered event');
        this.setMutator(new Blockly.Mutator(['agocontrol_contentConditionMutator']));
    },
    
    mutationToDom: function() {
        if( !this._conditionCount ) {
            return null;
        }
        var container = document.createElement('mutation');
        container.setAttribute('conditioncount', this._conditionCount);
        return container;
    },
    
    domToMutation: function(xmlElement) {
        this._conditionCount = parseInt(xmlElement.getAttribute('conditioncount'), 10);
        for( var i=1; i<=this._conditionCount; i++ )
        {
            this.appendValueInput("PROP"+i)
                .setCheck("Boolean")
                .setAlign(Blockly.ALIGN_RIGHT)
                .appendField(new Blockly.FieldDropdown([["and", "AND"], ["or", "OR"]]), "COND"+i);
        }
    },
    
    decompose: function(workspace) {
        //build container
        var containerBlock = Blockly.Block.obtain(workspace, 'agocontrol_contentMutator');
        containerBlock.initSvg();
        var connection = containerBlock.getInput('STACK').connection;
        for( var i=0; i<this._conditionCount; i++ )
        {
            var condition = Blockly.Block.obtain(workspace, 'agocontrol_contentConditionMutator');
            condition.initSvg();
            connection.connect(condition.previousConnection);
            connection = condition.nextConnection;
        }
        return containerBlock;
    },
    
    compose: function(containerBlock) {
        //clear everything
        for( var i=this._conditionCount; i>0; i--)
        {
            this.removeInput('PROP'+i);
        }
        this._conditionCount = 0;
      
        //rebuild the block's optional inputs
        var clauseBlock = containerBlock.getInputTargetBlock('STACK');
        while( clauseBlock )
        {
            this._conditionCount++;
            var propInput = this.appendValueInput("PROP"+this._conditionCount)
                                .setCheck("Boolean")
                                .setAlign(Blockly.ALIGN_RIGHT)
                                .appendField(new Blockly.FieldDropdown([["and", "AND"], ["or", "OR"]]), "COND"+this._conditionCount);
            //reconnect any child blocks
            if( clauseBlock.valueConnection_ )
            {
                propInput.connection.connect(clauseBlock.valueConnection_);
            }
            clauseBlock = clauseBlock.nextConnection && clauseBlock.nextConnection.targetBlock();
        }
    },
    
    /**
     * Store pointers to any connected child blocks.
     * @param {!Blockly.Block} containerBlock Root block in mutator.
     * @this Blockly.Block
     */
    saveConnections: function(containerBlock) {
        var clauseBlock = containerBlock.getInputTargetBlock('STACK');
        var x = 1;
        while (clauseBlock)
        {
            var inputProp = this.getInput('PROP'+x);
            clauseBlock.valueConnection_ = inputProp && inputProp.connection.targetConnection;
            x++;
            clauseBlock = clauseBlock.nextConnection && clauseBlock.nextConnection.targetBlock();
        }
    },
    
    onchange: function() {
        if( !this.workspace )
            return;
            
        //update event blocks
        window.BlocklyAgocontrol.updateEventBlocks(this);
    }
};

Blockly.Blocks['agocontrol_contentMutator'] = {
  /**
   * Mutator block for if container.
   * @this Blockly.Block
   */
    init: function() {
        this.setColour(210);
        this.container = this.appendDummyInput().appendField("triggered event is");
        this.appendStatementInput('STACK');
        this.setInputsInline(true);
        this.contextMenu = false;
    }
};

//content block mutator
Blockly.Blocks['agocontrol_contentConditionMutator'] = {
    init: function() {
        this.setColour(210);
        this.container = this.appendDummyInput()
            .appendField("property condition");
        this.setPreviousStatement(true);
        this.setNextStatement(true);
        this.setTooltip("Add condition");
        this.contextMenu = false;
    }
};

//get triggered event property (content in agocontrol language)
Blockly.Blocks['agocontrol_contentProperty'] = {
    init: function() {
        //members
        this.firstRun = true;
        this.properties = undefined;
        this.lastEvent = undefined;

        //block definition
        //this.setHelpUrl('TODO');
        this.setColour(140);
        this.container = this.appendDummyInput()
            .appendField("content prop")
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getAllEvents()), "EVENT")
            .appendField(new Blockly.FieldDropdown([['','']]), "PROP");
        this.setInputsInline(true);
        this.setOutput(true); //no way to determine output
        this.setTooltip('Return content property');
    },
  
    //force event selected in dropdown
    setEvent: function(newEvent) {
        var field = this.getField_("EVENT");
        if( field )
        {
            return field.setValue(newEvent);
        }
    },
  
    //return current event
    getEvent: function() {
        return this.getFieldValue("EVENT");
    },

    //onchange event
    onchange: function() {
        if( !this.workspace )
            return;
            
        //update event blocks
        window.BlocklyAgocontrol.updateEventBlocks(this);
        
        //check if block is nested in "content" block
        if( !this.inContent )
            this.setWarningText('Content property block MUST be nested in a "content" logic block');
        else
            this.setWarningText(null);
            
        //update properties list
        var currentEvent = this.getFieldValue("EVENT");
        var currentProp = null;
        if( this.firstRun )
        {
            currentProp = this.getFieldValue("PROP");
        }
        if( this.lastEvent!=currentEvent )
        {
            this.lastEvent = currentEvent;
            var events = window.BlocklyAgocontrol.getEventProperties(currentEvent);
            if( events.length===0 )
                events.push(['','']);
            this.container.removeField("PROP");
            this.container.appendField(new Blockly.FieldDropdown(events), "PROP");
            
            //select property value if necessary
            if( currentProp )
            {
                var field = this.getField_("PROP");
                if( field )
                {
                    field.setValue(currentProp);
                }
            }
        }
        
        this.firstRun = false;
    }
};

//range block
Blockly.Blocks['agocontrol_range'] = {
    init: function() {
        //this.setHelpUrl('TODO');
        this.setColour(230);
        this.appendValueInput("MIN")
            .setCheck("Number");
        this.appendDummyInput()
            .appendField(new Blockly.FieldDropdown([["<", "LT"], ["<=", "LTE"]]), "SIGN_MIN");
        this.appendValueInput("PROP")
            .setCheck("String");
        this.appendDummyInput()
            .appendField(new Blockly.FieldDropdown([["<", "LT"], ["<=", "LTE"]]), "SIGN_MAX");
        this.appendValueInput("MAX")
            .setCheck("Number");
        this.setInputsInline(true);
        this.setOutput(true, "Boolean");
        this.setTooltip('Return true if property is in range');
    },

    onchange: function() {
        //TODO check min max
    }
};

//list of values options
Blockly.Blocks['agocontrol_valueOptions'] = {
    init: function() {
        //members
        this.currentValue = null;
       
        //block definition
        //this.setHelpUrl("TODO");
        this.setColour(160);
        this.setOutput(true, 'String');
        this.container = this.appendDummyInput()
            .appendField(new Blockly.FieldDropdown(window.BlocklyAgocontrol.getAllValues(true)), 'VALUE')
            .appendField(new Blockly.FieldDropdown([['','']]), 'OPTIONS');
        this.setTooltip("Return selected value option");
    },
    
    onchange: function() {
        if( !this.workspace )
            return;
            
        //update values list
        var currentValue = this.getFieldValue("VALUE");
        if( this.currentValue!=currentValue )
        {
            this.currentValue = currentValue;
            var value = window.BlocklyAgocontrol.getValue(currentValue);
            var options = [];
            if( value.type!==null && value.type==="option" && value.options!==null && value.options.length>0 )
            {
                for( var i=0; i<value.options.length; i++)
                {
                    options.push([value.options[i], value.options[i]]);
                }
            }
            if( options.length===0 )
                options.push(['','']);
            this.container.removeField("OPTIONS");
            this.container.appendField(new Blockly.FieldDropdown(options), "OPTIONS");
        }
    },

    //return selected option
    getSelectedOption: function() {
        return this.getFieldValue("OPTIONS") || '';
    }
};

