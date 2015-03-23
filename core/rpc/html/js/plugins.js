/**
 * plugins.js file contains specific code that need to interact with main ui, ie:
 *  - SecurityPlugin: catch security event and display pin panel to disable alarm if triggered
 *  - <add your plugin's description here>
 *
 * How to use:
 *  - Create your own plugin object at the end of this file
 *  - Append new instance of your object in initPlugins function below
 *  - If you need to add some html content, use dedicated file /templates/plugins/plugins.html
 */

//Init custom behaviour
Agocontrol.prototype.initPlugins = function()
{
    var plugins = {};
    plugins["security"] = new SecurityPlugin(this);
    return plugins;
};

/**
 * Security plugin
 * Display pin panel to disarm alarm
 */
function SecurityPlugin(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.securityController = null;
    self.currentPin = ko.observable('');
    self.alarmStateInterval = null;
    self.countdown = ko.observable(-1);
    self.securityIsRunning = true;

    //get security controller
    self.getSecurityControllerUuid = function()
    {
        //check if security module is running
        if( self.agocontrol.processes().length>0 )
        {
            //process list loaded
            var found = false;
            for( var i=0; i<self.agocontrol.processes().length; i++ )
            {
                if( self.agocontrol.processes()[i].name==='agosecurity' )
                {
                    //security found
                    if( self.agocontrol.processes()[i].running==0 )
                    {
                        self.securityIsRunning = false;
                    }
                    found = true;
                    break;
                }
            }

            if( !found )
            {
                self.securityIsRunning = false;
            }
        }

        //remove event handler is security is not running
        if( !self.securityIsRunning )
        {
            self.agocontrol.removeEventHandler(self.eventHandler);
            //no need to get controller uuid, stop here
            return;
        }

        //get uuid
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=='securitycontroller' )
            {
                self.securityController = self.agocontrol.devices()[i].uuid;
                break;
            }
        }

    };

    //event handler, need to catch
    self.eventHandler = function(event)
    {
        //get controller uuid if necessary
        if( !self.securityController )
        {
            self.getSecurityControllerUuid();
        }

        if( event.event=='event.security.countdown.started' || event.event=='event.security.countdown' )
        {
            //open pin code panel
            self.openPanel();
            //handle countdown
            self.countdown(event.delay);
        }
        else if( event.event=='event.security.alarmcanceled' )
        {
            //alarm has been canceled elsewhere, close panel
            self.closePanel();
        }
    };
    self.agocontrol.addEventHandler(self.eventHandler);

    //key pressed on pin panel
    self.keyPressed = function(key)
    {
        //append key to current pin
        self.currentPin( self.currentPin()+key );

        //get security controller uuid if necessary
        if( !self.securityController )
        {
            self.getSecurityControllerUuid();
        }

        //cancel alarm if necessary
        if( self.currentPin().length==4 )
        {
            //block panel
            self.agocontrol.block($('#securityPanel'));

            var content = {};
            content.uuid = self.securityController;
            content.command = 'cancelalarm';
            content.pin = self.currentPin();
            self.agocontrol.sendCommand(content, function(res) {
                //reset pin code
                self.resetPin();

                //check result
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.result && res.result.result.code && res.result.result.code=="success" )
                    {
                        notif.success('Alarm disarmed');
                        self.closePanel();
                    }
                    else
                    {
                        notif.error('Wrong pin code!');
                    }
                }
                else
                {
                    notif.error('Unable to cancel alarm!');
                }

                //unblock panel
                self.agocontrol.unblock($('#securityPanel'));
            });
        }
    };

    //reset pin
    self.resetPin = function()
    {
        self.currentPin('');
    };

    //open pin panel
    self.openPanel = function()
    {
        $('#securityPanel').addClass('active');
    };

    //close pin panel
    self.closePanel = function()
    {
        $('#securityPanel').removeClass('active');
    };
};

