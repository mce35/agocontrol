Array.prototype.chunk = function(chunkSize) {
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

    //load template
    self.loadTemplate = function(template)
    {
        //block ui
        $.blockUI({ message: '<img src="img/loading.gif" />  Loading...' });

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
                    yepnope({
                        load : resources,
                        complete : function() {
                            // here, all resources are really loaded
                            if (typeof init_template == 'function')
                            {
                                var model = init_template(template.path+'/', template.params, self.agocontrol);
                                if( model )
                                {
                                    self.currentView(new View(template.path+'/'+template.template, model));
                                }
                                else
                                {
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
                    });
                }
            }
            else
            {
                if (typeof init_template == 'function')
                {
                    var model = init_template(template.path+'/', template.params, self.agocontrol);
                    if( model )
                    {
                        self.currentView(new View(template.path+'/'+template.template, model));
                    }
                    else
                    {
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
                var basePath = "plugins/" + plugin._name;
                self.loadTemplate(new Template(basePath, plugin.resources, plugin.template, null));
            }
            else
            {
                notif.fatal('Specified plugin not found!');
            }
        });

        //configuration presentation loading
        this.get('#config', function()
        {
            var basePath = 'configuration/presentation';
            self.loadTemplate(new Template(basePath, null, 'html/presentation', null));
        });

        //configuration loading
        this.get('#config/:name', function()
        {
            //get config infos
            var config = null;
            for( var i=0; i<self.agocontrol.configurations().length; i++ )
            {
                if( self.agocontrol.configurations()[i].name==this.params.name )
                {
                    //config found
                    config = self.agocontrol.configurations()[i];
                }
            }
            if( config )
            {
                self.loadTemplate(new Template(config.path, null, config.template, null));
            }
            else
            {
                notif.fatal('Specified config page not found');
            }
        });

        //help
        this.get('#help/:name', function()
        {
            var basePath = 'help/' + this.params.name;
            var templatePath = '/html/' + this.params.name;
            self.loadTemplate(new Template(basePath, null, templatePath, null));
        });

        //startup page (dashboard)
        this.get('', function ()
        {
            this.app.runRoute('get', '#dashboard/all');
        });
    }).run();

    self.agocontrol.subscribe();
};

ko.applyBindings(new AgocontrolViewModel());

