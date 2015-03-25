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
 * Formats a date object
 * 
 * @param date
 * @param simple
 * @returns {String}
 */
function formatDate(date)
{
    return date.toLocaleDateString() + " " + date.toLocaleTimeString();
};

/**
 * Format specified string to lower case except first letter to uppercase
 */
function ucFirst(str)
{
    return str.charAt(0).toUpperCase() + str.slice(1).toLowerCase();
}

/**
 * Convert string under format "d.m.y h:m" to js Date object
 */
function stringToDatetime(str)
{
    var dt = str.match(/(\d+).(\d+).(\d+)\s+(\d+):(\d+)/);
    return new Date(dt[3], parseInt(dt[2])-1, dt[1], dt[4], dt[5]);
}

/**
 * Convert datetime js object to string under format "d.m.y h:m"
 */
function datetimeToString(dt)
{
    var str = $.datepicker.formatDate('dd.mm.yy', dt);
    str += ' ';
    str += ( dt.getHours()<10 ? '0'+dt.getHours() : dt.getHours() );
    str += ':';
    str += ( dt.getMinutes()<10 ? '0'+dt.getMinutes() : dt.getMinutes() );
    return str;
}

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
    var dfd = $.Deferred();

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

    // Attach handlers for all browsers
    script.onload = script.onreadystatechange = function( _, isAbort ) {
        if ( isAbort || !script.readyState || /loaded|complete/.test( script.readyState ) ) {
            cleanup();

            // Callback if not abort
            if ( !isAbort ) {
                dfd.resolve();
            }
        }
    };

    script.onerror = function() {
        // Unfortunately no meaningfull errors here
        cleanup();
        dfd.reject();
    };

    // Circumvent IE6 bugs with base elements (#2709 and #4378) by prepending
    // Use native DOM manipulation to avoid our domManip AJAX trickery
    head.insertBefore( script, head.firstChild );

    return dfd.promise();
}

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
}

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

    //disable click event
    self.noclick = function()
    {
        return false;
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
    };

    self.gotoApplication = function(application)
    {
        location.hash = 'app/' + application.name;
    };

    self.gotoHelp = function(help)
    {
        location.hash = 'help/' + help.name;
    };

    //load template
    self.loadTemplate = function(template)
    {
        //block ui
        $.blockUI({ message: '<img src="img/loading.gif" />  Loading...' });

        //reset current template
        if( typeof reset_template == 'function' )
        {
            reset_template(self.currentModel);
        }

        //reset some old stuff
        delete reset_template;
        delete init_template;
        delete afterrender_template;

        // Load template script file
        var scriptPromise = loadScript(template.path+'/template.js');
        var resourcePromise;

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
                var dfd = $.Deferred();
                resourcePromise = dfd.promise();
                head.load(resources, function() {
                    // head.load does not have any failure mechanism
                    dfd.resolve();
                });
            }
        }

        // Compound promise which fires when both above are ready
        $.when(scriptPromise, resourcePromise)
            .then(function(){
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
                $.unblockUI();
            })
            .fail(function(d1, d2) {
                // Note that jquery fails our compound promise as soon as ANY fails.
                $.unblockUI();
                console.error("Failed to load template or template resources");
                notif.fatal('Problem during page loading.');
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
        });

    /* While waiting, configure routes using sammy.js framework */
    sammyApp = Sammy(function(){
        //load ui plugins
        self.plugins = self.agocontrol.initPlugins();

        //dashboard loading
        this.get('#dashboard/:name', function()
        {
            if( this.params.name==='all' )
            {
                //special case for main dashboard (all devices)
                var basePath = 'dashboard/all';
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', null));
            }
            else
            {
                var dashboard = self.agocontrol.getDashboard(this.params.name)
                if(dashboard) {
                    var basePath = 'dashboard/custom';
                    self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:false}));
                }else{
                    notif.fatal('Specified custom dashboard not found!');
                }
            }
        });

        //custom dashboard edition
        this.get('#dashboard/:name/edit', function()
        {
            var dashboard = self.agocontrol.getDashboard(this.params.name)
            if(dashboard) {
                var basePath = 'dashboard/custom';
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:true}));
            }else{
                notif.fatal('Specified custom dashboard not found!');
            }
        });

        //application loading
        this.get('#app/:name', function()
        {
            // Apps may be loaded async; getApplication returns a promise
            self.agocontrol.getApplication(this.params.name)
                .then(function(application){
                        var basePath = "applications/" + application.dir;
                        self.loadTemplate(new Template(basePath, application.resources, application.template, null));
                    },
                    function(err){
                        // XXX: notif is not available here
                        notif.fatal('Specified application not found!');
                    }
                );
        });

        //configuration loading
        this.get('#config/:name', function()
        {
            //get config infos
            var config = null;
            for( var category in self.agocontrol.configurations() )
            {
                if( self.agocontrol.configurations()[category].subMenus===null )
                {
                    if( self.agocontrol.configurations()[category].menu.name==this.params.name )
                    {
                        config = self.agocontrol.configurations()[category].menu;
                        break;
                    }
                }
                else
                {
                    for( var i=0; i<self.agocontrol.configurations()[category].subMenus.length; i++ )
                    {
                        if( self.agocontrol.configurations()[category].subMenus[i].name==this.params.name )
                        {
                            config = self.agocontrol.configurations()[category].subMenus[i];
                            break;
                        }
                    }
                    if( config!=null )
                    {
                        break;
                    }
                }
            }
            if( config )
            {
                var basePath = "configuration/" + config.dir;
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

