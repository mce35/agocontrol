'use strict';

goog.provide('Blockly.Blocks.datetime');

goog.require('Blockly.Blocks');

Blockly.Blocks['datetime_timestamp'] = {
  /**
   * Block for current timestamp
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendDummyInput()
        .appendField("timestamp");
    this.setOutput(true, "Number");
    this.setTooltip('Return current timestamp');
  }
};

Blockly.Blocks['datetime_totimestamp'] = {
  /**
   * Block for datetime to timestamp conversion
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("DATETIME")
        .setCheck("String")
        .appendField("datetime");
    this.appendValueInput("PATTERN")
        .setCheck("String")
        .appendField("with pattern");
    this.setInputsInline(true);
    this.setOutput(true, "Number");
    this.setTooltip('Convert datetime to timestamp according to specified datetime pattern');
  }
};

Blockly.Blocks['datetime_currentdate'] = {
  /**
   * Block for date format
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("FORMAT")
        .setCheck("String")
        .appendField("format date to");
    this.setInputsInline(true);
    this.setOutput(true, "String");
    this.setTooltip('Format current date to specified format');
  }
};

Blockly.Blocks['datetime_specificdate'] = {
  /**
   * Block for date format
   * @this Blockly.Block
   */
  init: function() {
    //this.setHelpUrl('http://www.example.com/');
    this.setColour(20);
    this.appendValueInput("TIMESTAMP")
        .setCheck("Number")
        .appendField("format timestamp");
    this.appendValueInput("FORMAT")
        .setCheck("String")
        .appendField("to");
    this.setInputsInline(true);
    this.setOutput(true, "String");
    this.setTooltip('Format timestamp to specified format');
  }
};