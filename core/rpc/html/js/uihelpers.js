/**
 * Append UI helpers to Agocontrol object
 */

//Block specified DOM element
Agocontrol.prototype.block = function(el)
{
    $(el).block({ message: '<img src="img/loading.gif" />' });
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
function getErrorMessage(error) {
    if(!error)
        throw new Error("Cannot get error from nothing!");

    if(error.data && error.data.description)
        return error.data.description;

    if(error.message)
        // Should be a non-human readable error, such as "no.reply"
        return error.message;

    return "JSON-RPC error " + error.code;
}

/* Show an JSON-RPC error using notif
 * use on sendCommmand promise like this:
 *
 *  promise.catch(notifCommandError)
 */
function notifCommandError(error) {
    notif.error("ERROR: " + getErrorMessage(error));
}

//Init specific knockout bindings
Agocontrol.prototype.initSpecificKnockoutBindings = function()
{
    //dashboard widget slider bindings
    ko.bindingHandlers.slider = {
        init : function(element, valueAccessor, allBindingsAccessor) {
            var options = allBindingsAccessor().sliderOptions || {};
            $(element).slider(options);
            ko.utils.registerEventHandler(element, "slidechange", function(event, ui) {
                var observable = valueAccessor();
                observable(ui.value);
                // Hack to avoid setting the level on startup
                // So we call the syncLevel method when we have
                // a mouse event (means user triggered).
                if (options.dev && event.clientX)
                {
                    options.dev.syncLevel();
                }
            });
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).slider("destroy");
            });
        },
        update : function(element, valueAccessor) {
            var value = ko.unwrap(valueAccessor());
            if (isNaN(value))
            {
                value = 0;
            }
            $(element).slider("value", value);
        }
    };

    //bootstrap tabs
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
            }
        }
    };

    //bootstrap toggle switch
    ko.bindingHandlers.toggleSwitch = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            var accessor = valueAccessor();
            if( accessor.onSwitchChange && accessor.onSwitchChange instanceof Function )
            {
                var cb = accessor.onSwitchChange;
                if( accessor.data )
                {
                    //overwrite onSwitchChange callback to pass data
                    accessor.onSwitchChange = function(event, state) {
                        cb(event, state, accessor.data, element);
                    };
                }
                else
                {
                    //no data, pass element
                    accessor.onSwitchChange = function(event, state) {
                        cb(event, state, element);
                    };
                }
            }

            //init widget with specified options
            //@see options http://www.bootstrap-switch.org/options.html
            $(element).bootstrapSwitch(accessor);
        }
    };
};

/**
 * Return human readable size
 * http://stackoverflow.com/a/20463021/3333386
 */
function sizeToHRSize(a,b,c,d,e)
{
    return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes');
}

/**
 * convert timestamp to human readable datetime
 */
function timestampToHRstring(ts)
{
    var d = new Date();
    d.setTime(ts*1000);

    month = d.getMonth()+1;
    if( month<10 )
        month = '0'+month;

    day = d.getDate();
    if( day<10 )
        day = '0'+day;

    hours = d.getHours();
    if( hours<10 )
        hours = '0'+hours;

    minutes = d.getMinutes();
    if( minutes<10 )
        minutes = '0'+minutes;

    seconds = d.getSeconds();
    if( seconds<10 )
        seconds = '0'+seconds;

    return ''+d.getFullYear()+'/'+month+'/'+day+' '+hours+':'+minutes+':'+seconds;
}

