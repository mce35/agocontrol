'use strict';

goog.provide('Blockly.Blocks.common');

goog.require('Blockly.Blocks');
  
Blockly.Blocks['common_tonumber'] = {
  /**
   * Block for value number casting
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("VALUE")
        .appendField("toNumber");
    this.setOutput(true, "Number");
    this.setTooltip('Cast input to number');
  }
};

Blockly.Blocks['common_tostring'] = {
  /**
   * Block for value string casting
   * @this Blockly.Block
   */
   init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("VALUE")
        .appendField("toString");
    this.setOutput(true, "String");
    this.setTooltip('Cast input to string');
  }
};

Blockly.Blocks['common_type'] = {
  /**
   * Block for item type
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("ITEM")
        .appendField("type of");
    this.setOutput(true, "String");
    this.setTooltip('Returns type of input');
  }
};

Blockly.Blocks['common_execute'] = {
  /**
   * Block for command execution
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("CMD")
        .setCheck("String")
        .appendField("execute");
    this.setPreviousStatement(true);
    this.setNextStatement(true);
    this.setTooltip('Execute command');
  }
};

Blockly.Blocks['text_format'] = {
  /**
   * Block for test formatting
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(160);
    this.appendValueInput("ITEM")
        .appendField("format");
    this.appendValueInput("FORMAT")
        .setCheck("String")
        .appendField("to");
    this.setInputsInline(true);
    this.setOutput(true, "String");
    this.setTooltip('Format input to specified format');
  }
};

