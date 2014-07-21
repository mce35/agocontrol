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
 * @fileoverview Generating Lua for common blocks.
 * @author ellen.spertus@gmail.com (Ellen Spertus)
 */
'use strict';

goog.provide('Blockly.Lua.common');

goog.require('Blockly.Lua');


Blockly.Lua['common_tonumber'] = function(block) {
  var code = 'tonumber(' + Blockly.Lua.valueToCode(block, 'VALUE', Blockly.Lua.ORDER_ATOMIC) + ')';
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['common_tostring'] = function(block) {
  var code = 'tostring(' + Blockly.Lua.valueToCode(block, 'VALUE', Blockly.Lua.ORDER_ATOMIC) + ')';
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['common_type'] = function(block) {
  var code = 'type(' + Blockly.Lua.valueToCode(block, 'ITEM', Blockly.Lua.ORDER_ATOMIC) + ')';
  return [code, Blockly.Lua.ORDER_ATOMIC];
};

Blockly.Lua['common_execute'] = function(block) {
  var code = 'os.execute(' + Blockly.Lua.valueToCode(block, 'CMD', Blockly.Lua.ORDER_ATOMIC) + ')';
  return code;
};
