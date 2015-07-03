/**
 * alert plugin
 * @returns {agoalert}
 */
function AlertPlugin(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.smsStatus = ko.observable(self.smsStatus);
    self.selectedSmsProvider = ko.observable(self.selectedSmsProvider);
    self.twelvevoipUsername = ko.observable(self.twelvevoipUsername);
    self.twelvevoipPassword = ko.observable(self.twelvevoipPassword);
    self.freemobileUser = ko.observable(self.freemobileUser);
    self.freemobileApikey = ko.observable(self.freemobileApikey);
    self.mailStatus = ko.observable(self.mailStatus);
    self.mailSmtp = ko.observable(self.mailSmtp);
    self.mailLogin = ko.observable(self.mailLogin);
    self.mailPassword = ko.observable(self.mailPassword);
    self.mailTls = ko.observable(self.mailTls);
    self.mailSender = ko.observable(self.mailSender);
    self.twitterStatus = ko.observable(self.twitterStatus);
    self.pushStatus = ko.observable(self.pushStatus);
    self.selectedPushProvider = ko.observable(self.selectedPushProvider);
    self.pushbulletSelectedDevices = ko.observableArray();
    self.pushbulletAvailableDevices = ko.observableArray();
    self.pushbulletApikey = ko.observable();
    self.pushoverUserid = ko.observable();
    self.pushoverToken = ko.observable();
    self.nmaApikey = ko.observable(self.nmaApikey);
    self.nmaAvailableApikeys = ko.observableArray();
    self.nmaSelectedApikeys = ko.observableArray();
    self.agoalertUuid;
    
    //get agoalert uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='alertcontroller' )
            {
                self.agoalertUuid = devices[i].uuid;
            }
        }
    }

    //get current status
    self.getAlertsConfigs = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'status';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.mailStatus(res.data.mail.configured);
                if (res.data.mail.configured)
                {
                    self.mailSmtp(res.data.mail.smtp);
                    self.mailLogin(res.data.mail.login);
                    self.mailPassword(res.data.mail.password);
                    if (res.data.mail.tls == 1)
                        self.mailTls(true);
                    else
                        self.mailTls(false);
                    self.mailSender(res.data.mail.sender);
                }
                self.smsStatus(res.data.sms.configured);
                if (res.data.sms.configured)
                {
                    self.selectedSmsProvider(res.data.sms.provider);
                    if( res.data.sms.provider=='12voip' )
                    {
                        self.twelvevoipUsername(res.data.sms.username);
                        self.twelvevoipPassword(res.data.sms.password);
                    }
                    else if( res.data.sms.provider=='freemobile' )
                    {
                        self.freemobileUser(res.data.sms.user);
                        self.freemobileApikey(res.data.sms.apikey);
                    }
                }
                self.twitterStatus(res.data.twitter.configured);
                self.pushStatus(res.data.push.configured);
                if (res.data.push.configured)
                {
                    self.selectedPushProvider(res.data.push.provider);
                    if (res.data.push.provider == 'pushbullet')
                    {
                        self.pushbulletApikey(res.data.push.apikey);
                        self.pushbulletAvailableDevices(res.data.push.devices);
                        self.pushbulletSelectedDevices(res.data.push.devices);
                    }
                    else if (res.data.push.provider == 'pushover')
                    {
                        self.pushoverUserid(res.data.push.userid);
                        self.pushoverToken(res.data.push.token);
                    }
                    else if (res.data.push.provider == 'notifymyandroid')
                    {
                        self.nmaAvailableApikeys(res.data.push.apikeys);
                    }
                }
            });
    };

    this.twitterUrl = function()
    {
        var el = $('#twitterUrl');
        if( el===undefined )
        {
            notif.error('#ie');
            return;
        }
        var generatingUrl = $('#gu') || 'generating url';
        var authorizationUrl = $('#au') || 'authorization url';

        //get authorization url
        el.html(generatingUrl.html()+'...');
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'twitter';
        content.accesscode = '';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if (res.data.error == 0)
                {
                    //display link
                    el.html('<a href="' + res.data.url + '" target="_blank">'+authorizationUrl.html()+'</a>');
                }
                else
                {
                    el.html('error');
                    notif.error('Unable to get Twitter url');
                }
            });
    };

    this.twitterAccessCode = function()
    {
        var el = $('#twitterAccessCode');
        if( el===undefined )
        {
            notif.error('#ie');
            return;
        }
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'twitter';
        content.accesscode = el.val();
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.getAlertsConfigs();
            });
    };

    this.twitterTest = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'test';
        content.type = 'twitter';
        self.agocontrol.sendCommand(content);
    };

    this.smsConfig = function() {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'sms';
        content.provider = self.selectedSmsProvider();
        if( self.selectedSmsProvider()=='12voip' )
        {
            content.username = self.twelvevoipUsername();
            content.password = self.twelvevoipPassword();
        }
        else if( self.selectedSmsProvider()=='freemobile' )
        {
            content.user = self.freemobileUser();
            content.apikey = self.freemobileApikey();
        }
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.getAlertsConfigs();
            });
    };

    this.smsTest = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'test';
        content.type = 'sms';
        self.agocontrol.sendCommand(content);
    };

    this.mailConfig = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'mail';
        content.smtp = self.mailSmtp();
        content.sender = self.mailSender();
        content.loginpassword = '';
        if( self.mailLogin()!==undefined )
        {
            content.loginpassword += self.mailLogin();
        }
        content.loginpassword += '%_%';
        if( self.mailPassword()!==undefined )
        {
            content.loginpassword += self.mailPassword();
        }
        content.tls = '0';
        if (self.mailTls())
        {
            content.tls = '1';
        }
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.getAlertsConfigs();
            });
    };

    this.mailTest = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'test';
        content.type = 'mail';
        content.tos = document.getElementsByClassName("mailEmail")[0].value;
        self.agocontrol.sendCommand(content);
    };

    this.pushbulletRefreshDevices = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'push';
        content.provider = this.selectedPushProvider();
        content.subcmd = 'getdevices';
        content.apikey = self.pushbulletApikey();
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.pushbulletAvailableDevices(res.data.devices);
            });
    };

    this.nmaAddApikey = function()
    {
        if ( self.nmaApikey() && self.nmaApikey().length > 0)
        {
            self.nmaAvailableApikeys.push(self.nmaApikey());
        }
        self.nmaApikey('');
    };

    this.nmaDelApikey = function()
    {
        for ( var j = self.nmaSelectedApikeys().length - 1; j >= 0; j--)
        {
            for ( var i = self.nmaAvailableApikeys().length - 1; i >= 0; i--)
            {
                if (self.nmaAvailableApikeys()[i] === self.nmaSelectedApikeys()[j])
                {
                    self.nmaAvailableApikeys().splice(i, 1);
                }
            }
        }
        self.nmaAvailableApikeys(self.nmaAvailableApikeys());
    };

    this.pushConfig = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'setconfig';
        content.type = 'push';
        content.provider = this.selectedPushProvider();
        if (this.selectedPushProvider() == 'pushbullet')
        {
            content.subcmd = 'save';
            content.apikey = this.pushbulletApikey();
            content.devices = this.pushbulletSelectedDevices();
        }
        else if (this.selectedPushProvider() == 'pushover')
        {
            content.userid = this.pushoverUserid();
            content.token = this.pushoverToken();
        }
        else if (this.selectedPushProvider() == 'notifymyandroid')
        {
            content.apikeys = this.nmaAvailableApikeys();
        }
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.getAlertsConfigs();
            });
    };

    this.pushTest = function()
    {
        var content = {};
        content.uuid = self.agoalertUuid;
        content.command = 'test';
        content.type = 'push';
        self.agocontrol.sendCommand(content);
    };

    //get config
    self.getAlertsConfigs();
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new AlertPlugin(agocontrol.devices(), agocontrol);
    return model;
}
