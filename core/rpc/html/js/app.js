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

    self.gotoPlugin = function(plugin)
    {
        location.hash = 'plugin/' + plugin.name;
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

        //load template script file
        $.getScript(template.path+'/template.js' , function()
        {
            //Load the plugins resources if any
            if( template.resources && ((template.resources.css && template.resources.css.length>0) || (template.resources.js && template.resources.js.length>0)) )
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
                    head.load(resources, function() {
                        // here, all resources are really loaded
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
                    });
                }
            }
            else
            {
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
            }
        }).fail(function(jqXHR, textStatus, errorThrown) {
            $.unblockUI();
            console.error("Failed to load template: ["+textStatus+"] "+errorThrown.message);
            console.error(errorThrown);
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

    //configure routes using sammy.js framework
    Sammy(function ()
    {
        //load agocontrol inventory
        self.agocontrol.getInventory(null, false);

        function getDashboard(name)
        {
            var dashboard = null;
            for( var i=0; i<self.agocontrol.dashboards().length; i++ )
            {
                if( self.agocontrol.dashboards()[i].name==name )
                {
                    dashboard = self.agocontrol.dashboards()[i];
                    break;
                }
            }
            return dashboard;
        };

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
                var dashboard = getDashboard(this.params.name);
                if( dashboard )
                {
                    var basePath = 'dashboard/custom';
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
            var dashboard = getDashboard(this.params.name);
            if( dashboard )
            {
                var basePath = 'dashboard/custom';
                self.loadTemplate(new Template(basePath, null, 'html/dashboard', {dashboard:dashboard, edition:true}));
            }
            else
            {
                notif.fatal('Specified custom dashboard not found!');
            }
        });

        //plugin loading
        this.get('#plugin/:name', function()
        {
            //get plugin infos from members
            var plugin = null;
            for( var i=0; i<self.agocontrol.plugins().length; i++ )
            {
                if( self.agocontrol.plugins()[i].name==this.params.name )
                {
                    //plugin found
                    plugin = self.agocontrol.plugins()[i];
                    break;
                }
            }
            if( plugin )
            {
                var basePath = "plugins/" + plugin.dir;
                self.loadTemplate(new Template(basePath, plugin.resources, plugin.template, null));
            }
            else
            {
                notif.fatal('Specified plugin not found!');
            }
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
    }).run();

    self.agocontrol.subscribe();
    self.agocontrol.initSpecificKnockoutBindings();
};

ko.applyBindings(new AgocontrolViewModel());

