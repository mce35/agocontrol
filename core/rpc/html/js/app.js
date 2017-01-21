/*============== ARRAY FUNCTIONS ==============*/
/**
 * Add chunk function to Array object
 */
Array.prototype.chunk = function(chunkSize)
{
    var array = this;
    return [].concat.apply([], array.map(function(elem, i) {
        return i % chunkSize ? [] : [ array.slice(i, i + chunkSize) ];
    }));
};

// Polyfill for Array.prototype:
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/find
// https://tc39.github.io/ecma262/#sec-array.prototype.find
if (!Array.prototype.find) {
  Object.defineProperty(Array.prototype, 'find', {
    value: function(predicate) {
     // 1. Let O be ? ToObject(this value).
      if (this == null) {
        throw new TypeError('"this" is null or not defined');
      }

      var o = Object(this);

      // 2. Let len be ? ToLength(? Get(O, "length")).
      var len = o.length >>> 0;

      // 3. If IsCallable(predicate) is false, throw a TypeError exception.
      if (typeof predicate !== 'function') {
        throw new TypeError('predicate must be a function');
      }

      // 4. If thisArg was supplied, let T be thisArg; else let T be undefined.
      var thisArg = arguments[1];

      // 5. Let k be 0.
      var k = 0;

      // 6. Repeat, while k < len
      while (k < len) {
        // a. Let Pk be ! ToString(k).
        // b. Let kValue be ? Get(O, Pk).
        // c. Let testResult be ToBoolean(? Call(predicate, T, « kValue, k, O »)).
        // d. If testResult is true, return kValue.
        var kValue = o[k];
        if (predicate.call(thisArg, kValue, k, o)) {
          return kValue;
        }
        // e. Increase k by 1.
        k++;
      }

      // 7. Return undefined.
      return undefined;
    }
  });
}

// Production steps of ECMA-262, Edition 5, 15.4.4.18
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach
// Reference: http://es5.github.io/#x15.4.4.18
if (!Array.prototype.forEach) {

    Array.prototype.forEach = function(callback, thisArg) {

        var T, k;

        if (this === null) {
            throw new TypeError(' this is null or not defined');
        }

        // 1. Let O be the result of calling toObject() passing the
        // |this| value as the argument.
        var O = Object(this);

        // 2. Let lenValue be the result of calling the Get() internal
        // method of O with the argument "length".
        // 3. Let len be toUint32(lenValue).
        var len = O.length >>> 0;

        // 4. If isCallable(callback) is false, throw a TypeError exception.
        // See: http://es5.github.com/#x9.11
        if (typeof callback !== "function") {
            throw new TypeError(callback + ' is not a function');
        }

        // 5. If thisArg was supplied, let T be thisArg; else let
        // T be undefined.
        if (arguments.length > 1) {
            T = thisArg;
        }

        // 6. Let k be 0
        k = 0;

        // 7. Repeat, while k < len
        while (k < len) {

            var kValue;

            // a. Let Pk be ToString(k).
            //    This is implicit for LHS operands of the in operator
            // b. Let kPresent be the result of calling the HasProperty
            //    internal method of O with argument Pk.
            //    This step can be combined with c
            // c. If kPresent is true, then
            if (k in O) {

                // i. Let kValue be the result of calling the Get internal
                // method of O with argument Pk.
                kValue = O[k];

                // ii. Call the Call internal method of callback with T as
                // the this value and argument list containing kValue, k, and O.
                callback.call(T, kValue, k, O);
            }
            // d. Increase k by 1.
            k++;
        }
        // 8. return undefined
    };
}


/**
 * Allow adding multiple items to a observableArray in one shot,
 * without firing an event for each new item
 */
ko.observableArray.fn.pushAll = function(valuesToPush) {
    var underlyingArray = this();
    this.valueWillMutate();
    ko.utils.arrayPushAll(underlyingArray, valuesToPush);
    this.valueHasMutated();
    return this;  //optional
};

/**
 * Remove all items, then adding multiple items to a observableArray in one shot,
 * without firing an event for each change
 */
ko.observableArray.fn.replaceAll = function(valuesToPush) {
    var underlyingArray = this();
    this.valueWillMutate();
    underlyingArray.splice(0, underlyingArray.length);
    ko.utils.arrayPushAll(underlyingArray, valuesToPush);
    this.valueHasMutated();
    return this;  //optional
};

/**
 * Find the first item identified by the given predicate function.
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/find
 */
ko.observableArray.fn.find = function(predicate) {
    var underlyingArray = this();
    return underlyingArray.find(predicate);
};

ko.observableArray.fn.forEach = function(callback, thisArg) {
    var underlyingArray = this();
    return underlyingArray.forEach(callback, thisArg);
};


/*============== NUMBER FUNCTIONS ==============*/
/**
 * Round a number to specified number of decimals
 * @see http://www.jacklmoore.com/notes/rounding-in-javascript/
 */
function roundNumber(value, decimals)
{
    return Number(Math.round(value+'e'+decimals)+'e-'+decimals);
};

/**
 * Return human readable size
 * http://stackoverflow.com/a/20463021/3333386
 */
function sizeToHRSize(a,b,c,d,e)
{
    return (b=Math,c=b.log,d=1e3,e=c(a)/c(d)|0,a/b.pow(d,e)).toFixed(2) +' '+(e?'kMGTPEZY'[--e]+'B':'Bytes');
};


/*============== STRING FUNCTIONS ==============*/
/**
 * Format specified string to lower case except first letter to uppercase
 */
function ucFirst(str)
{
    return str.charAt(0).toUpperCase() + str.slice(1).toLowerCase();
};


/*============== DATETIME FUNCTIONS ==============*/
/**
 * Return date format according to user locale
 */
function getDateFormat()
{
    var formats = {
        "ar-SA" : "dd/MM/yy", "bg-BG" : "dd.M.yyyy", "ca-ES" : "dd/MM/yyyy", "zh-TW" : "yyyy/M/d",
        "cs-CZ" : "d.M.yyyy", "da-DK" : "dd-MM-yyyy", "de-DE" : "dd.MM.yyyy", "el-GR" : "d/M/yyyy",
        "en-US" : "M/d/yyyy", "fi-FI" : "d.M.yyyy", "fr-FR" : "dd/MM/yyyy", "he-IL" : "dd/MM/yyyy",
        "hu-HU" : "yyyy. MM. dd.", "is-IS" : "d.M.yyyy", "it-IT" : "dd/MM/yyyy", "ja-JP" : "yyyy/MM/dd",
        "ko-KR" : "yyyy-MM-dd", "nl-NL" : "d-M-yyyy", "nb-NO" : "dd.MM.yyyy", "pl-PL" : "yyyy-MM-dd",
        "pt-BR" : "d/M/yyyy", "ro-RO" : "dd.MM.yyyy", "ru-RU" : "dd.MM.yyyy", "hr-HR" : "d.M.yyyy",
        "sk-SK" : "d. M. yyyy", "sq-AL" : "yyyy-MM-dd", "sv-SE" : "yyyy-MM-dd", "th-TH" : "d/M/yyyy",
        "tr-TR" : "dd.MM.yyyy", "ur-PK" : "dd/MM/yyyy", "id-ID" : "dd/MM/yyyy", "uk-UA" : "dd.MM.yyyy",
        "be-BY" : "dd.MM.yyyy", "sl-SI" : "d.M.yyyy", "et-EE" : "d.MM.yyyy", "lv-LV" : "yyyy.MM.dd.",
        "lt-LT" : "yyyy.MM.dd", "fa-IR" : "MM/dd/yyyy", "vi-VN" : "dd/MM/yyyy", "hy-AM" : "dd.MM.yyyy",
        "az-Latn-AZ" : "dd.MM.yyyy", "eu-ES" : "yyyy/MM/dd", "mk-MK" : "dd.MM.yyyy", "af-ZA" : "yyyy/MM/dd",
        "ka-GE" : "dd.MM.yyyy", "fo-FO" : "dd-MM-yyyy", "hi-IN" : "dd-MM-yyyy", "ms-MY" : "dd/MM/yyyy",
        "kk-KZ" : "dd.MM.yyyy", "ky-KG" : "dd.MM.yy", "sw-KE" : "M/d/yyyy", "uz-Latn-UZ" : "dd/MM yyyy",
        "tt-RU" : "dd.MM.yyyy", "pa-IN" : "dd-MM-yy", "gu-IN" : "dd-MM-yy", "ta-IN" : "dd-MM-yyyy",
        "te-IN" : "dd-MM-yy", "kn-IN" : "dd-MM-yy", "mr-IN" : "dd-MM-yyyy", "sa-IN" : "dd-MM-yyyy",
        "mn-MN" : "yy.MM.dd", "gl-ES" : "dd/MM/yy", "kok-IN" : "dd-MM-yyyy", "syr-SY" : "dd/MM/yyyy",
        "dv-MV" : "dd/MM/yy", "ar-IQ" : "dd/MM/yyyy", "zh-CN" : "yyyy/M/d", "de-CH" : "dd.MM.yyyy",
        "en-GB" : "dd/MM/yyyy", "es-MX" : "dd/MM/yyyy", "fr-BE" : "d/MM/yyyy", "it-CH" : "dd.MM.yyyy",
        "nl-BE" : "d/MM/yyyy", "nn-NO" : "dd.MM.yyyy", "pt-PT" : "dd-MM-yyyy", "sr-Latn-CS" : "d.M.yyyy",
        "sv-FI" : "d.M.yyyy", "az-Cyrl-AZ" : "dd.MM.yyyy", "ms-BN" : "dd/MM/yyyy", "uz-Cyrl-UZ" : "dd.MM.yyyy",
        "ar-EG" : "dd/MM/yyyy", "zh-HK" : "d/M/yyyy", "de-AT" : "dd.MM.yyyy", "en-AU" : "d/MM/yyyy",
        "es-ES" : "dd/MM/yyyy", "fr-CA" : "yyyy-MM-dd", "sr-Cyrl-CS" : "d.M.yyyy", "ar-LY" : "dd/MM/yyyy",
        "zh-SG" : "d/M/yyyy", "de-LU" : "dd.MM.yyyy", "en-CA" : "dd/MM/yyyy", "es-GT" : "dd/MM/yyyy",
        "fr-CH" : "dd.MM.yyyy", "ar-DZ" : "dd-MM-yyyy", "zh-MO" : "d/M/yyyy", "de-LI" : "dd.MM.yyyy",
        "en-NZ" : "d/MM/yyyy", "es-CR" : "dd/MM/yyyy", "fr-LU" : "dd/MM/yyyy", "ar-MA" : "dd-MM-yyyy",
        "en-IE" : "dd/MM/yyyy", "es-PA" : "MM/dd/yyyy", "fr-MC" : "dd/MM/yyyy", "ar-TN" : "dd-MM-yyyy",
        "en-ZA" : "yyyy/MM/dd", "es-DO" : "dd/MM/yyyy", "ar-OM" : "dd/MM/yyyy", "en-JM" : "dd/MM/yyyy",
        "es-VE" : "dd/MM/yyyy", "ar-YE" : "dd/MM/yyyy", "en-029" : "MM/dd/yyyy", "es-CO" : "dd/MM/yyyy",
        "ar-SY" : "dd/MM/yyyy", "en-BZ" : "dd/MM/yyyy", "es-PE" : "dd/MM/yyyy", "ar-JO" : "dd/MM/yyyy",
        "en-TT" : "dd/MM/yyyy", "es-AR" : "dd/MM/yyyy", "ar-LB" : "dd/MM/yyyy", "en-ZW" : "M/d/yyyy",
        "es-EC" : "dd/MM/yyyy", "ar-KW" : "dd/MM/yyyy", "en-PH" : "M/d/yyyy", "es-CL" : "dd-MM-yyyy",
        "ar-AE" : "dd/MM/yyyy", "es-UY" : "dd/MM/yyyy", "ar-BH" : "dd/MM/yyyy", "es-PY" : "dd/MM/yyyy",
        "ar-QA" : "dd/MM/yyyy", "es-BO" : "dd/MM/yyyy", "es-SV" : "dd/MM/yyyy", "es-HN" : "dd/MM/yyyy",
        "es-NI" : "dd/MM/yyyy", "es-PR" : "dd/MM/yyyy", "am-ET" : "d/M/yyyy", "tzm-Latn-DZ" : "dd-MM-yyyy",
        "iu-Latn-CA" : "d/MM/yyyy", "sma-NO" : "dd.MM.yyyy", "mn-Mong-CN" : "yyyy/M/d", "gd-GB" : "dd/MM/yyyy",
        "en-MY" : "d/M/yyyy", "prs-AF" : "dd/MM/yy", "bn-BD" : "dd-MM-yy", "wo-SN" : "dd/MM/yyyy",
        "rw-RW" : "M/d/yyyy", "qut-GT" : "dd/MM/yyyy", "sah-RU" : "MM.dd.yyyy", "gsw-FR" : "dd/MM/yyyy",
        "co-FR" : "dd/MM/yyyy", "oc-FR" : "dd/MM/yyyy", "mi-NZ" : "dd/MM/yyyy", "ga-IE" : "dd/MM/yyyy",
        "se-SE" : "yyyy-MM-dd", "br-FR" : "dd/MM/yyyy", "smn-FI" : "d.M.yyyy", "moh-CA" : "M/d/yyyy",
        "arn-CL" : "dd-MM-yyyy", "ii-CN" : "yyyy/M/d", "dsb-DE" : "d. M. yyyy", "ig-NG" : "d/M/yyyy",
        "kl-GL" : "dd-MM-yyyy", "lb-LU" : "dd/MM/yyyy", "ba-RU" : "dd.MM.yy", "nso-ZA" : "yyyy/MM/dd",
        "quz-BO" : "dd/MM/yyyy", "yo-NG" : "d/M/yyyy", "ha-Latn-NG" : "d/M/yyyy", "fil-PH" : "M/d/yyyy",
        "ps-AF" : "dd/MM/yy", "fy-NL" : "d-M-yyyy", "ne-NP" : "M/d/yyyy", "se-NO" : "dd.MM.yyyy",
        "iu-Cans-CA" : "d/M/yyyy", "sr-Latn-RS" : "d.M.yyyy", "si-LK" : "yyyy-MM-dd", "sr-Cyrl-RS" : "d.M.yyyy",
        "lo-LA" : "dd/MM/yyyy", "km-KH" : "yyyy-MM-dd", "cy-GB" : "dd/MM/yyyy", "bo-CN" : "yyyy/M/d",
        "sms-FI" : "d.M.yyyy", "as-IN" : "dd-MM-yyyy", "ml-IN" : "dd-MM-yy", "en-IN" : "dd-MM-yyyy",
        "or-IN" : "dd-MM-yy", "bn-IN" : "dd-MM-yy", "tk-TM" : "dd.MM.yy", "bs-Latn-BA" : "d.M.yyyy",
        "mt-MT" : "dd/MM/yyyy", "sr-Cyrl-ME" : "d.M.yyyy", "se-FI" : "d.M.yyyy", "zu-ZA" : "yyyy/MM/dd",
        "xh-ZA" : "yyyy/MM/dd", "tn-ZA" : "yyyy/MM/dd", "hsb-DE" : "d. M. yyyy", "bs-Cyrl-BA" : "d.M.yyyy",
        "tg-Cyrl-TJ" : "dd.MM.yy", "sr-Latn-BA" : "d.M.yyyy", "smj-NO" : "dd.MM.yyyy", "rm-CH" : "dd/MM/yyyy",
        "smj-SE" : "yyyy-MM-dd", "quz-EC" : "dd/MM/yyyy", "quz-PE" : "dd/MM/yyyy", "hr-BA" : "d.M.yyyy.",
        "sr-Latn-ME" : "d.M.yyyy", "sma-SE" : "yyyy-MM-dd", "en-SG" : "d/M/yyyy", "ug-CN" : "yyyy-M-d",
        "sr-Cyrl-BA" : "d.M.yyyy", "es-US" : "M/d/yyyy"
    };
    var lang = navigator.language || navigator.userLanguage;
    if( navigator.language.indexOf('-')===-1 )
    {
        lang = navigator.language+'-'+navigator.language.toUpperCase();
    }
    return formats[lang] || 'dd/MM/yyyy';
};

/**
 * Return time format according to user locale
 */
function getTimeFormat(withSeconds)
{
    var d = new Date();
    var t = d.toLocaleTimeString();
    if( t.indexOf('AM')!==-1 || t.indexOf('PM')!==-1 )
    {
        if( withSeconds===undefined )
        {
            return 'hh:mm a';
        }
        else
        {
            return 'hh:mm:ss a';
        }
    }
    else
    {
        if( withSeconds===undefined )
        {
            return 'hh:mm';
        }
        else
        {
            return 'hh:mm:ss';
        }
    }
};

/**
 * Return date and time format according to user locale
 */
function getDateTimeFormat(withSeconds)
{
    return getDateFormat()+' '+getTimeFormat(withSeconds);
}

/**
 * Convert string to js Date object. If conversion fails return current date
 * /!\ Please use ISO string format to make sure conversion is correct!
 */
function isoStringToDatetime(str)
{
    var dt = Date.parse(str);
    if( isNaN(dt) )
    {
        return new Date();
    }
    return new Date(dt);
};

/**
 * Convert datetime js object to string according to user locales
 */
function dateToString(dt)
{
    return dt.toLocaleDateString();
}

/**
 * Convert datetime js object to string according to user locales
 */
function timeToString(dt)
{
    var t = dt.toLocaleTimeString();
    var re = /(\d+):(\d+):\d+(.*)/g;
    var m;
    if( (m=re.exec(t))!==null )
    {
        //remove seconds
        return m[1]+':'+m[2]+m[3];
    }
    return t;
};

/**
 * Convert datetime js object to string according to user locales
 */
function datetimeToString(dt)
{
    return dateToString(dt)+' '+timeToString(dt);
};

/**
 * Convert timestamp to string under format "d.m.y h:m"
 */
function timestampToString(ts)
{
    var dt = new Date(ts*1000);
    return datetimeToString(dt);
};

/**
 * Return current timestamp (in seconds)
 */
function getTimestamp()
{
    return Math.floor(Date.now()/1000);
};


/*============== AGOCONTROL FUNCTIONS ==============*/
/**
 * Loads a new JavaScript using <script> element.
 *
 * This is used instead of $.ajax(), since ajax only uses <script> elements
 * if we force it to run crossdomain. And if we do not load it with script headers,
 * Chrome debugger fails to find the script.
 * Why not use that? Because by default, crossdomain <script> loads does not handle onError,
 * and thus jQuery does not implement that...
 *
 * This is inspired by jQuery's 'script' transport.
 */
function loadScript(url) {
    var head = document.head || jQuery("head")[0] || document.documentElement;
    var script = document.createElement("script");
    script.async = true;
    script.src = url;

    function cleanup(){
        // Handle memory leak in IE
        script.onload = script.onreadystatechange = null;

        // Remove the script
        if ( script.parentNode ) {
            script.parentNode.removeChild( script );
        }

        // Dereference the script
        script = null;
    }

    var promise = new Promise(function(resolve, reject) {
        // Attach handlers for all browsers
        script.onload = script.onreadystatechange = function( _, isAbort ) {
            if ( isAbort || !script.readyState || /loaded|complete/.test( script.readyState ) ) {
                cleanup();

                // Callback if not abort
                if ( !isAbort ) {
                    resolve();
                }
            }
        };

        script.onerror = function() {
            // Unfortunately no meaningfull errors here
            cleanup();
            reject();
        };
    });

    // Circumvent IE6 bugs with base elements (#2709 and #4378) by prepending
    // Use native DOM manipulation to avoid our domManip AJAX trickery
    head.insertBefore( script, head.firstChild );

    return promise;
};

//View object
//dynamic page loading inspired from : http://jsfiddle.net/rniemeyer/PctJz/
function View(templateName, model)
{
    this.templateName = templateName;
    this.model = model;
};

//Template object
function Template(path, resources, template, params)
{
    this.path = path;
    this.resources = resources;
    this.template = template;
    this.params = params; //data passed to loaded template
};

//Agocontrol viewmodel
//based on knockout webmail example
//http://learn.knockoutjs.com/WebmailExampleStandalone.html
function AgocontrolViewModel()
{
    //MEMBERS
    var self = this;
    self.currentView = ko.observable();
    self.agocontrol = new Agocontrol();
    self.currentModel = null;
    self.plugins = {};
    self.scriptPromise = null;
    self.resourcePromise = null;
    self.itemActivated = null;

    //disable click event
    self.noclick = function()
    {
        return false;
    };

    //hide control sidebar
    self.toggleControlSidebar = function()
    {
        $("[data-toggle='control-sidebar']").click();
    };

    //activate item on left bar
    self.activate = function(id)
    {
        if( self.itemActivated!==null )
        {
            //disable current activated item
            $(self.itemActivated).removeClass('active');
        }

        if( id!==null )
        {
            //activate item
            self.itemActivated = $('#menu_'+id.replace(' ','_'));
            self.itemActivated.addClass('active');
        }
    };

    //route functions
    self.gotoDashboard = function(dashboard, edit)
    {
        if( edit===true )
        {
            location.hash = 'dashboard/' + dashboard.name + '/edit';
        }
        else
        {
            location.hash = 'dashboard/' + dashboard.name;
        }
    };

    self.gotoConfiguration = function(config)
    {
        location.hash = 'config/' + config.name;
        self.toggleControlSidebar();
    };

    self.gotoApplication = function(application)
    {
        location.hash = 'app/' + application.name;
    };

    self.gotoProtocol = function(protocol)
    {
        location.hash = 'proto/' + protocol.name;
    };

    self.gotoHelp = function(help)
    {
        location.hash = 'help/' + help.name;
        self.toggleControlSidebar();
    };

    //load template
    self.loadTemplate = function(template)
    {
        //block ui
        $.blockUI({
            message: '<i class="fa fa-circle-o-notch fa-4x fa-spin" style="color:white;"/><br/><span style="color:white;">please wait...</span>',
            css : {
                background: 'none',
                border: 'none'
            }
        });

        //reset current template
        if( typeof reset_template == 'function' )
        {
            reset_template(self.currentModel);
        }

        //reset some old stuff
        reset_template = null;
        init_template = null;
        afterrender_template = null;

        //cancel previous template loading if necessary
        if( self.scriptPromise!==null )
        {
            self.scriptPromise.cancel('aborted');
            if( self.resourcePromise!==null )
            {
                self.resourcePromise.cancel('aborted');
            }
        }

        // Load template script file
        self.scriptPromise = loadScript(template.path+'/template.js');
        self.resourcePromise = null;

        // Load other required resources in parallel
        if( template.resources )
        {
            var resources = [];
            if (template.resources.css && template.resources.css.length > 0)
            {
                for ( var i = 0; i < template.resources.css.length; i++)
                {
                    resources.push(template.path + "/" + template.resources.css[i]);
                }
            }
            if (template.resources.js && template.resources.js.length > 0)
            {
                for ( var i = 0; i < template.resources.js.length; i++)
                {
                    resources.push(template.path + "/" + template.resources.js[i]);
                }
            }
            if (resources.length > 0)
            {
                self.resourcePromise = new Promise(function(resolve, reject){
                    head.load(resources, function() {
                        // head.load does not have any failure mechanism
                        resolve();
                    });
                });
            }
        }

        // Compound promise which fires when both above are ready
        Promise.all([self.scriptPromise, self.resourcePromise])
            .then(function() {
                // Template script and Resources ready
                if (typeof init_template == 'function')
                {
                    self.currentModel = init_template(template.path+'/', template.params, self.agocontrol);
                    if( self.currentModel )
                    {
                        self.currentView(new View(template.path+'/'+template.template, self.currentModel));
                    }
                    else
                    {
                        self.currentModel = null;
                        console.error('Failed to load template: no model returned by init_template. Please check your code.');
                        notif.fatal('Problem during page loading.');
                    }
                }
                else
                {
                    console.error('Failed to load template: init_template function not defined. Please check your code.');
                    notif.fatal('Problem during page loading.');
                }
            })
            .catch(Promise.CancellationError, function(err) {
                //aborted
                //XXX seems not be triggered when cancel() is called :S
            })
            .catch(function(err) {
                if( err==='aborted' )
                {
                    //page loading aborted. Nothing else to do
                }
                else
                {
                    // Note that jquery fails our compound promise as soon as ANY fails.
                    console.error("Failed to load template or template resources", err);
                    notif.fatal('Problem during page loading: '+ err);
                }
            })
            .finally(function() {
                self.scriptPromise = null;
                self.resourcePromise = null;
                $.unblockUI();
            });
    };

    //call afterrender_template if available.
    //the afterrender_template can be directly called from index.html but to make it understandable, we put here the call
    self.afterRender = function() {
        if( typeof afterrender_template == 'function' )
        {
            afterrender_template(self.currentModel);
        }
    };

    //call afterRender function of plugins if available
    self.afterRenderPlugins = function() {
        for( var plugin in self.plugins )
        {
            if( typeof self.plugins[plugin].afterRender == 'function' )
            {
                self.plugins[plugin].afterRender();
            }
        }
    };

    var sammyApp;
    /* Initialize agocontrol inventory.
     * when basic init is done, fire up sammy app*/
    self.agocontrol.initialize()
        .then(function() {
            sammyApp.run();
        })
        .then(function() {
            //everything is loaded, init here all needed stuff
        });

    /* While waiting, configure routes using sammy.js framework */
    sammyApp = Sammy(function(){
        //load ui plugins
        self.plugins = self.agocontrol.initPlugins();

        //dashboard
        this.get('#dashboard/:name', function()
        {
            if( this.params.name==='all' )
            {
                //special case for main dashboard (all devices)
                var basePath = 'dashboard/all';
                self.activate('all');
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', null));
            }
            else
            {
                var dashboard = self.agocontrol.getDashboard(this.params.name)
                if(dashboard)
                {
                    var basePath = 'dashboard/custom';
                    self.activate(dashboard.name);
                    self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:false}));
                }
                else
                {
                    notif.fatal('Specified custom dashboard not found!');
                }
            }
        });

        //custom dashboard edition
        this.get('#dashboard/:name/edit', function()
        {
            var dashboard = self.agocontrol.getDashboard(this.params.name)
            if(dashboard)
            {
                var basePath = 'dashboard/custom';
                self.activate(dashboard.name);
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:true}));
            }
            else
            {
                notif.fatal('Specified custom dashboard not found!');
            }
        });

        //application
        this.get('#app/:name', function()
        {
            // Apps may be loaded async; getApplication returns a promise
            self.agocontrol.getApplication(this.params.name)
                .then(function(application) {
                    var basePath = "applications/" + application.dir;
                    self.activate(application.name);
                    self.loadTemplate(new Template(basePath, application.resources, application.template, null));
                })
                .catch(function(err) {
                    notif.fatal('Specified application not found!');
                });
        });

        //protocol
        this.get('#proto/:name', function()
        {
            //Protocols may be loaded async; getProtocol returns a promise
            self.agocontrol.getProtocol(this.params.name)
                .then(function(protocol) {
                    var basePath = "protocols/" + protocol.dir;
                    self.activate(protocol.name);
                    self.loadTemplate(new Template(basePath, protocol.resources, protocol.template, null));
                })
                .catch(function(err){
                    notif.fatal('Specified protocol application not found!');
                });
        });

        //configuration
        this.get('#config/:name', function()
        {
            //get config infos
            var config = null;
            for( var i=0; i<self.agocontrol.configurations().length; i++ )
            {
                if( self.agocontrol.configurations()[i].name==this.params.name )
                {
                    config = self.agocontrol.configurations()[i];
                    break;
                }
            }
            if( config )
            {
                var basePath = "configuration/" + config.dir;
                self.activate(null);
                self.loadTemplate(new Template(basePath, config.resources, config.template, null));
            }
            else
            {
                notif.fatal('Specified config page not found');
            }
        });

        //help
        this.get('#help/:name', function()
        {
            var help = null;
            for( var i=0; i<self.agocontrol.helps().length; i++ )
            {
                if( self.agocontrol.helps()[i].name==this.params.name )
                {
                    //page found
                    help = self.agocontrol.helps()[i];
                    break;
                }
            }
            if( help )
            {
                var basePath = "help/" + help.dir;
                self.activate(null);
                self.loadTemplate(new Template(basePath, help.resources, help.template, null));
            }
            else
            {
                notif.fatal('Specified help page not found');
            }
        });

        //startup page (dashboard)
        this.get('', function ()
        {
            this.app.runRoute('get', '#dashboard/all');
        });
    });


    self.agocontrol.subscribe();
    self.agocontrol.initSpecificKnockoutBindings();
};

ko.applyBindings(new AgocontrolViewModel());

