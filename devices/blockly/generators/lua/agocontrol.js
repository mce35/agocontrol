'use strict';

goog.provide('Blockly.Lua.agocontrol');

goog.require('Blockly.Lua');

Blockly.Lua['agocontrol_deviceNo'] = function(block) {
    return ["''", Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_deviceName'] = function(block) {
    var uuid = Blockly.Lua.valueToCode(block, 'UUID', Blockly.Lua.ORDER_NONE) || '';
    var code = "getDeviceName("+uuid+")";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_deviceUuid'] = function(block) {
    var code = "'" + block.getFieldValue('DEVICE') + "'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_deviceInternalid'] = function(block) {
    var code = "'" + block.getFieldValue('DEVICE') + "'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_eventNo'] = function(block) {
    return ["''", Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_deviceEvent'] = function(block) {
    var code = "'" + block.getFieldValue('EVENT') + "'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_eventAll'] = function(block) {
    var code = "'" + block.getFieldValue('EVENT') + "'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_deviceProperty'] = function(block) {
    var selectedProp = block.getFieldValue("PROP");
    var propType = undefined;
    for( var prop in block.properties )
    {
        if( prop===selectedProp )
        {
            propType = block.properties[prop].type;
            break;
        }
    }
    var code = "";
    if( propType && propType==="dynamic" )
    {
        //dynamic property is typically a sensor value
        //TODO add possibility to get other value property (quantity, unit, latitude, longitude...) (replace level by specified one)
        code = "getDeviceInventory(\"" + block.lastDevice + "\",\"" + selectedProp + "\",\"" + "level" + "\")";
    }
    else
    {
        //device property
        code = "getDeviceInventory(\"" + block.lastDevice + "\",\"" + selectedProp + "\",\"\")";
    }
    return [code, Blockly.Lua.ORDER_ATOMIC];
};


Blockly.Lua['agocontrol_sendMessage'] = function(block) {
    var code = "";
    var value;
    var command = block.getFieldValue("COMMAND") || 'nil';
    var device = block.getFieldValue("DEVICE") || 'nil';
    var fields = block.getFields();
    code += "'command="+command+"', 'uuid="+device+"' ";
    for( var field in fields )
    {
        value = Blockly.Lua.valueToCode(block, field, Blockly.Lua.ORDER_NONE) || '';
        //always concat value to field name because of computed value (like string concat, operation...)
        code += ", '"+fields[field]+"=' .. "+value;
    }
    return "sendMessage("+code+")\n";
};

Blockly.Lua['agocontrol_printContent'] = function(block) {
    var code = "print 'Event content:'\n";
    code += "for key,value in pairs(content) do\n";
    code += "  print(' - ' .. key .. '=' .. value .. ' [' .. type(value) .. ']' )\n";
    code += "end\n";
    return code;
};

Blockly.Lua['agocontrol_content'] = function(block) {
    var code = "";
    code += "content.subject == ";
    code += Blockly.Lua.valueToCode(block, 'EVENT', Blockly.Lua.ORDER_NONE) || 'nil';
    for( var i=1; i<=block._conditionCount; i++ )
    {
        if( block.getFieldValue('COND'+i)=="OR" )
            code += " or ";
        else
            code += " and ";
        var e = Blockly.Lua.valueToCode(block, 'PROP'+i, Blockly.Lua.ORDER_NONE);
		  if(e)
           code += '(' + e + ')';
    }
    return [code, Blockly.Lua.ORDER_NONE];
};

Blockly.Lua['agocontrol_contentProperty'] = function(block) {
    var code = "";
    var prop = block.getFieldValue("PROP");
    code = "content." + prop;
    if( prop=='level' )
    {
        //level from agolua is a string, convert it to number
        code = "tonumber(" + code + ")";
    }
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_fixedItemsList'] = function(block) {
    var code = "'"+block.getSelectedItem()+"'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_email'] = function(block) {
  var code = Blockly.Lua.quote_(block.getFieldValue('EMAIL'));
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_phoneNumber'] = function(block) {
  var code = Blockly.Lua.quote_(block.getFieldValue('PHONE'));
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_getVariable'] = function(block) {
    var code = "getVariable(\"" + block.getVariable() + "\")";
    return [code, Blockly.Lua.ORDER_NONE];
};

Blockly.Lua['agocontrol_setVariable'] = function(block) {
    var code = "";
    var name = block.getVariable();
    if( name.length>0 )
    {
        code = "setVariable('"+name+"',";
        code += Blockly.Lua.valueToCode(block, 'VALUE', Blockly.Lua.ORDER_NONE) || 'nil';
        code += ")\n";
        return code;
    }
    return '';
};

Blockly.Lua['agocontrol_sleep'] = function(block) {
    var code = "";
    var dur = Blockly.Lua.valueToCode(block, 'DURATION', Blockly.Lua.ORDER_NONE) || 1;
    code = "local time_to = os.time() + "+dur+"\n";
    code += "while os.time() < time_to do end\n";
    return code;
};

Blockly.Lua['agocontrol_range'] = function(block) {
    var code = "(";
    var prop = Blockly.Lua.valueToCode(block, 'PROP', Blockly.Lua.ORDER_NONE) || '';
    code += prop;
    if( block.getFieldValue('SIGN_MIN')=="LT" )
        code += " > ";
    else
        code += " >= ";
    code += Blockly.Lua.valueToCode(block, 'MIN', Blockly.Lua.ORDER_NONE) || 1;
    code += " and ";
    code += prop;
    if( block.getFieldValue('SIGN_MAX')=="LT" )
        code += " < ";
    else
        code += " <= ";
    code += Blockly.Lua.valueToCode(block, 'MAX', Blockly.Lua.ORDER_NONE) || 100;
    code += ")";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_valueOptions'] = function(block) {
    var code = "'"+block.getSelectedOption()+"'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_defaultEmail'] = function(block) {
    var code = "'"+block.getEmail()+"'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_defaultPhoneNumber'] = function(block) {
    var code = "'"+block.getPhoneNumber()+"'";
    return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['agocontrol_journal'] = function(block) {
    var msg = Blockly.Lua.valueToCode(block, 'MSG', Blockly.Lua.ORDER_ATOMIC) || '';
    var type = block.getFieldValue('TYPE');
    var uuid = block.getJournalUuid();
    var code = "";
    if( uuid!==null )
    {
        code = "sendMessage('command=addmessage', 'uuid="+uuid+"' , 'message=' .. "+msg+", 'type="+type+"')\n"
    }
    else
    {
        //no journal
        code = "print('No journal available!')\n";
    }
    return code;
};

Blockly.Lua['agocontrol_weekday'] = function(block) {
    var day = Blockly.Lua.valueToCode(block, 'WEEKDAY', Blockly.Lua.ORDER_ATOMIC) || '';
    var type = block.getFieldValue('TYPE');
    var code = "";
    if( type=='-2' )
    {
        //weekday (1-5)
        code = ""+day+" <= '5'"
    }
    else if( type=='-1' )
    {
        //weekend (6,7)
        code = ""+day+" >= '6'"
    }
    else
    {
        //specific day
        code = ""+day+" == "+type
    }
    return [code, Blockly.Lua.ORDER_RELATIONAL];
};

