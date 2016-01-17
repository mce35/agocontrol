/**
 * Append UI helpers to Agocontrol object
 */

//Block specified DOM element
Agocontrol.prototype.block = function(el)
{
    $.blockUI.defaults.css = {};
    $(el).block({
        message: '<i class="fa fa-cog fa-3x fa-spin" style="color:white;"/>'
    });
};

//Unblock specified DOM element
Agocontrol.prototype.unblock = function(el)
{
    $(el).unblock();    
};

/* Tries to convert a JSON-RPC error object into a error string.
 *
 * If .data.description is set, this is used.
 * Else, if .message is set, this is used.
 * Else, we use .code (which is JSONRPC numeric).
 *
 * TODO: Do we want to use translated .message identifier 
 * primarily?
 */
function getErrorMessage(error)
{
    if(!error)
    {
        throw new Error("Cannot get error from nothing!");
    }

    if( error.data && error.data.description )
    {
        return error.data.description;
    }

    if( error.message )
    {
        // Should be a non-human readable error, such as "no.reply"
        return error.message;
    }

    return "JSON-RPC error " + error.code;
};

/* Show an JSON-RPC error using notif
 * use on sendCommmand promise like this:
 *
 *  promise.catch(notifCommandError)
 */
function notifCommandError(error)
{
    notif.error("ERROR: " + getErrorMessage(error));
};

//Init specific knockout bindings
Agocontrol.prototype.initSpecificKnockoutBindings = function()
{
    //bootstrap tabs bindings
    ko.bindingHandlers.bootstrapTabs = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //prevent from link redirection
            $(element).find('a').click(function(e) {
                e.preventDefault();
                $(this).tab('show');
            });

            //handle onChanged event
            var accessor = valueAccessor();
            if( accessor.onChanged && accessor.onChanged instanceof Function )
            {
                $(element).find('a').on('shown.bs.tab', function (e) {
                    //return selected tab index (first tab index is 0) and selected tab text (usually tab title)
                    accessor.onChanged($(e.target).closest('li').index(), $(e.target).text());
                });

                //launch callback on tab 0
                accessor.onChanged(0, $($(element).find('a')[0]).text());
            }
        }
    };

    //bootstrap toggle switch bindings
    ko.bindingHandlers.toggleSwitch = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            var options = valueAccessor();
            if( options.onSwitchChange && options.onSwitchChange instanceof Function )
            {
                var cb = options.onSwitchChange;
                if( options.data )
                {
                    //overwrite onSwitchChange callback to pass data
                    options.onSwitchChange = function(event, state) {
                        cb(event, state, options.data, element);
                    };
                }
                else
                {
                    //no data, pass element
                    options.onSwitchChange = function(event, state) {
                        cb(event, state, element);
                    };
                }
            }

            //init widget with specified options
            //@see options http://www.bootstrap-switch.org/options.html
            $(element).bootstrapSwitch(options);

            //clean up
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).bootstrapSwitch('destroy');
            });
        }
    };

    //bootstrap slider bindings
    //@see https://github.com/cosminstefanxp/bootstrap-slider-knockout-binding
    ko.bindingHandlers.sliderValue = {
        init: function (element, valueAccessor, allBindings, viewModel, bindingContext) {
            var params = valueAccessor();
    
            // Check whether the value observable is either placed directly or in the paramaters object.
            if (!(ko.isObservable(params) || params['value']))
                throw "You need to define an observable value for the sliderValue. Either pass the observable directly or as the 'value' field in the parameters.";
            if( !params['device'] )
            {
                throw "You need to set 'device' parameter to allow level saving.";
            }
    
            // Identify the value and initialize the slider
            var valueObservable;
            if (ko.isObservable(params)) {
                valueObservable = params;
                $(element).bootstrapSlider({value: ko.unwrap(params)});
            }
            else {
                valueObservable = params['value'];
                if (!Array.isArray(valueObservable)) {
                    // Replace the 'value' field in the options object with the actual value
                    params['value'] = ko.unwrap(valueObservable);
                    $(element).bootstrapSlider(params);
                } 
                else {
                    valueObservable = [params['value'][0], params['value'][1]];
                    params['value'][0] = ko.unwrap(valueObservable[0]);
                    params['value'][1] = ko.unwrap(valueObservable[1]);
                    $(element).bootstrapSlider(params);
                }
            }

            // Make sure we update the observable when changing the slider value
            $(element).on('slide', function (ev) {
                if (!Array.isArray(valueObservable))
                {
                    valueObservable(ev.value);
                } 
                else
                {
                    valueObservable[0](ev.value[0]);
                    valueObservable[1](ev.value[1]);
                }
            });

            // Only save value when user stops sliding
            $(element).on('slideStop', function (ev) {
                if (!Array.isArray(valueObservable))
                {
                    valueObservable(ev.value);
                    params['device'].syncLevel();
                } 
                else
                {
                    valueObservable[0](ev.value[0]);
                    valueObservable[1](ev.value[1]);
                }

            });
            
            // Clean up
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).bootstrapSlider('destroy');
                $(element).off('slide');
                $(element).off('slideStop');
            });
        },
        update: function (element, valueAccessor, allBindings, viewModel, bindingContext) {
            var modelValue = valueAccessor();
            var valueObservable;
            if (ko.isObservable(modelValue))
                valueObservable = modelValue;
            else 
                valueObservable = modelValue['value'];
    
            if (!Array.isArray(valueObservable)) {
                $(element).bootstrapSlider('setValue', parseFloat(valueObservable()));
            } 
            else {
                $(element).bootstrapSlider('setValue', [parseFloat(valueObservable[0]()),parseFloat(valueObservable[1]())]);
            }
        }
    };

    //bootstrap color picker bindings
    ko.bindingHandlers.colorpicker = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            var options = valueAccessor();

            if( !options['color'] || !ko.isObservable(options['color']) || options['color']().length!=3 )
            {
                throw "You need to define an observable value for the colorpicker. Pass the observable as the 'color' field in the options. Observable must be an array of 3 values (R-G-B).";
            }
            if( !options['device'] )
            {
                throw "You need to set 'device' parameter to allow color saving.";
            }

            var colorObservable = options['color'];
            //replace 'color' field by appropriate value
            options['color'] = 'rgb('+colorObservable()[0]+','+colorObservable()[1]+','+colorObservable()[2]+')';
            
            //init widget with specified options
            //@see options here: http://mjolnic.com/bootstrap-colorpicker/
            options['format'] = 'rgb';
            $(element).colorpicker(options);

            $(element).on('changeColor', function(ev) {
                //update observable
                var rgb = ev.color.toRGB();
                colorObservable([rgb.r, rgb.g, rgb.b]);
            });

            $(element).on('hidePicker', function(ev) {
                //update observable
                var rgb = ev.color.toRGB();
                colorObservable([rgb.r, rgb.g, rgb.b]);
                //and sync color
                options['device'].syncColor();
            });

            //clean up
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).off('changeColor');
                $(element).off('hidePicker');
                $(element).colorpicker('destroy');
            });
        }
    };

    ko.bindingHandlers.select2 = {
        init: function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //var allBindings = allBindingsAccessor();
            var options = valueAccessor();

            //handle options
            $(element).select2(options);
            
            //clean up
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).select2('destroy');
            });
        }
    };

    ko.bindingHandlers.formNoEnter = {
        init: function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            $(element).keypress(function(ev) {
                //prevent from enter key pressed in form
                if( ev.which=='13' )
                {
                    ev.preventDefault();
                }
            });
        }
    };
};

/**
 * Return current date time format to datetimepicker format
 */
function getDatetimepickerFormat()
{
    var d = getDateFormat();
    var t = getTimeFormat();
    var l = new Date().toLocaleTimeString();
    if( l.indexOf('AM')!==-1 || l.indexOf('PM')!==-1 )
    {
        //12h format
        return d.replace(/d+/gi,'d').replace(/m+/gi,'m').replace(/yyyy/gi,'Y').replace(/yy/gi,'y') + ' ' + t.replace(/h+/gi,'h').replace(/m+/gi,'i').replace(/a/gi,'A');
    }
    else
    {
        //24h format
        return d.replace(/d+/gi,'d').replace(/m+/gi,'m').replace(/yyyy/gi,'Y').replace(/yy/gi,'y') + ' ' + t.replace(/h+/gi,'H').replace(/m+/gi,'i');
    }
};

/**
 * Return device type according to bootstrap
 * @see https://github.com/titosust/Bootstrap-device-detector
 */
function getDeviceSize()
{
    var env = ["xs", "sm", "md", "lg"];
    var device = env[3];
    var $el = $('<div>').appendTo('body');
    for (var i = env.length - 1; i >= 0; i--)
    {
        $el.addClass('hidden-'+env[i]);
        if ($el.is(':hidden'))
        {
            device=env[i];
            break;
        }
    }
    $el.remove();
    return device;
};

