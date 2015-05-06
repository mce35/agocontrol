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
    notif.error("ERROR: " + getErrorMessage(msg));
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

    //gumby modal bindings
    ko.bindingHandlers.gumbyModal = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //handle close button
            $('.close.switch').click(function() {
                $(this).parents('.modal').removeClass('active');
                return false; //prevent from bubbling
            });
        }
    };

    //gumby tabs bindings
    ko.bindingHandlers.gumbyTabs = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //handle tab changing
            $(element).find('.tab-nav li > a').click(function() {
                //update style
                $(element).find(".tab-nav li").each(function() {
                    $(this).removeClass('active');
                });
                $(this).parent().addClass('active');

                this.$el = $(element);
                var index = $(this).parent().index();
                this.$content = this.$el.find(".tab-content");
                this.$nav = $(this).parent().find('li');
                this.$nav.add(this.$content).removeClass("active");
                this.$nav.eq(index).add(this.$content.eq(index)).addClass("active");
                return false; //prevent from bubbling
            });
        }
    };

};

/**
 * Return human readable size
 * http://stackoverflow.com/a/20463021/3333386
 */
function sizeToHRSize(a,b,c,d,e)
{
    return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes')
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

