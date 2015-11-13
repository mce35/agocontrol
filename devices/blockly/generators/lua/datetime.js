/**
 * Visual Blocks Language
 *
 * Copyright 2012 Google Inc.
 * http://blockly.googlecode.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Generating Lua for datetime blocks.
 * @author tanguy.bonneau@gmail.com
 */
'use strict';

goog.provide('Blockly.Lua.datetime');

goog.require('Blockly.Lua');


Blockly.Lua['datetime_timestamp'] = function(block) {
  var code = 'os.time()'
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['datetime_totimestamp'] = function(block) {
  var funcName = 'datetimeToTimestamp';
  if( !Blockly.Lua.definitions_[funcName] )
  {
    var func = 'function '+funcName+'(adatetime, apattern)\n'
    func += 'local runyear, runmonth, runday, runhour, runminute, runseconds = adatetime:match(apattern)\n';
    func += 'return os.time({year = runyear, month = runmonth, day = runday, hour = runhour, min = runminute, sec = runseconds})';
    func += 'end\n';
    code = Blockly.Lua.scrub_(block, func);
    Blockly.Lua.definitions_[funcName] = code;
  }
  
  var pattern = Blockly.Lua.valueToCode(block, 'PATTERN', Blockly.Lua.ORDER_ATOMIC);
  pattern = pattern.replace('\\','');
  var code = funcName+'(';
  code += Blockly.Lua.valueToCode(block, 'DATETIME', Blockly.Lua.ORDER_ATOMIC);
  code += ',';
  code += pattern;
  code += ')';
  
  return [code, Blockly.Lua.ORDER_ATOMIC];
}

Blockly.Lua['datetime_currentdate'] = function(block) {
  var format = Blockly.Lua.valueToCode(block, 'FORMAT', Blockly.Lua.ORDER_ATOMIC);
  format = format.replace('\\','');
  var code = 'os.date(' + format + ')';
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['datetime_specificdate'] = function(block) {
  var format = Blockly.Lua.valueToCode(block, 'FORMAT', Blockly.Lua.ORDER_ATOMIC);
  format = format.replace('\\','');
  var code = 'os.date(' + format + ',' + Blockly.Lua.valueToCode(block, 'TIMESTAMP', Blockly.Lua.ORDER_ATOMIC) + ')';
  return [code, Blockly.Lua.ORDER_ATOMIC];
};
