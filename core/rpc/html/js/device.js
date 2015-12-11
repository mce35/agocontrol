/**
 * Device class used by app.js and others for representing and interaction
 * with devices
 * 
 * @param obj
 * @param uuid
 * @returns {device}
 */
function device(agocontrol, obj, uuid) {
    var self = this;
    self.agocontrol = agocontrol;
    for ( var k in obj)
    {
        this[k] = obj[k];
    }

    this.uuid = uuid;
    this.action = ''; // dummy for table
    this.name = ko.observable(this.name);
    this.handledBy = this['handled-by'];
    var currentState = parseInt(this.state);
    this.state = ko.observable(currentState);
    this.values = ko.observable(this.values);
    this.timeStamp = ko.observable(datetimeToString(new Date(this.lastseen * 1000)));

    var params = [];
    for( var key in self.parameters )
    {
        var param = {};
        param.key = key;
        param.value = ko.observable(self.parameters[key]);
        params.push(param);
    }
    self.koparameters = ko.observableArray(params);

    this.stale = ko.observable(this.stale);
    self.staleStyle = ko.pureComputed(function() {
        return (self.stale() ? 'bg-red' : 'bg-light-blue');
    });

    if( this.devicetype=="dimmer" || this.devicetype=="dimmerrgb" )
    {
        this.level = ko.observable(currentState);
        this.state.subscribe(function(v){
            this.level(v);
        }, this);
        this.syncLevel = function() {
            var content = {};
            content.uuid = uuid;
            content.command = "setlevel";
            content.level = self.level();
            self.agocontrol.sendCommand(content);
        };
    }

    if (this.devicetype == "agocontroller")
    {
        self.agocontrol.agoController = uuid;
    }
    else if (this.devicetype == "eventcontroller")
    {
        self.agocontrol.eventController = uuid;
    }
    else if (this.devicetype == "scenariocontroller")
    {
        self.agocontrol.scenarioController = uuid;
    }
    else if (this.devicetype == "systemcontroller")
    {
        self.agocontrol.systemController = uuid;
    }
    else if( this.devicetype=="journal" )
    {
        self.agocontrol.journal = uuid;
    }

    if( this.devicetype=="dimmerrgb" )
    {
        self.color = ko.observableArray([255,0,0]);

        self.syncColor = function() {
            var content = {};
            content.uuid = uuid;
            content.command = "setcolor";
            content.red = self.color()[0];
            content.green = self.color()[1];
            content.blue = self.color()[2];
            self.agocontrol.sendCommand(content);
        };
    }

    this.multigraphThumb = ko.observable();
    this.getMultigraphThumb = function(deferred)
    {
        var content = {};
        content.command = "getthumb";
        content.uuid = self.agocontrol.dataLoggerController;
        content.multigraph = deferred.internalid;
        self.agocontrol.sendCommand(content, null, 10)
            .then(function(res) {
                deferred.observable('data:image/png;base64,' + res.result.data.graph);
            })
            .catch(function(err) {
                //no thumb available
                //TODO notif something?
                console.error('request getthumb failed');
            });
    };

    //refresh dashboard multigraph thumb
    self.refreshMultigraphThumbs = function()
    {
        //clear call to this function if no thumb on dashboard
        if( self.agocontrol.multigraphThumbs.length==0 )
        {
            window.clearInterval(self.agocontrol.refreshMultigraphThumbsInterval);
        }

        for( var i=0; i<self.agocontrol.multigraphThumbs.length; i++ )
        {
            if( !self.agocontrol.multigraphThumbs[i].removed )
            {
                self.getMultigraphThumb(self.agocontrol.multigraphThumbs[i]);
            }
        }
    };

    if (this.devicetype == "dataloggercontroller")
    {
        self.agocontrol.dataLoggerController = uuid;
        //load deferred thumbs
        for( var i=0; i<self.agocontrol.deferredMultigraphThumbs.length; i++ )
        {
            self.getMultigraphThumb(self.agocontrol.deferredMultigraphThumbs[i]);
        }
        self.agocontrol.deferredMultigraphThumbs = [];
        //auto resfresh thumbs periodically
        self.agocontrol.refreshMultigraphThumbsInterval = window.setInterval(self.refreshMultigraphThumbs, 120000);
    }

    if( self.devicetype=="multigraph" && $.trim(self.name()).length>0 )
    {
        var def = {'uuid': uuid, 'internalid':obj.internalid, 'observable':self.multigraphThumb, 'removed':false};
        if( self.agocontrol.dataLoggerController )
        {
            //get thumb right now
            self.getMultigraphThumb(def);
        }
        else
        {
            //defer thumb loading
            self.agocontrol.deferredMultigraphThumbs.push(def);
        }
        self.agocontrol.multigraphThumbs.push(def);
    }

    if (this.devicetype.match(/sensor$/) || this.devicetype.match(/meter$/) || this.devicetype.match(/thermostat$/) || this.devicetype=="multigraph" )
    {
        //fill values list
        this.valueList = ko.computed(function()
        {
            var result = [];
            for ( var k in self.values())
            {
                var unit = self.values()[k].unit;
                if (self.agocontrol.schema().units[self.values()[k].unit] !== undefined)
                {
                    unit = self.agocontrol.schema().units[self.values()[k].unit].label;
                }
                //fix unit if nothing specified
                if( $.trim(unit).length==0 )
                {
                    unit = '-';
                }
                if( self.values()[k].level!==null && self.values()[k].level!==undefined )
                {
                    result.push({
                        name : k.charAt(0).toUpperCase() + k.substr(1),
                        level : self.values()[k].level,
                        unit : unit,
                        levelUnit : ''+self.values()[k].level+unit
                    });
                }
                else if( self.values()[k].latitude && self.values()[k].longitude )
                {
                    result.push({
                        name : k.charAt(0).toUpperCase() + k.substr(1),
                        latitude : self.values()[k].latitude,
                        longitude : self.values()[k].longitude
                        //unit : no unit available for gps sensor
                    });
                }
            }
            return result;
        });
   
        //add function to get rrd graph
        this.getRrdGraph = function(uuid, start, end)
        {
            var content = {};
            content.command = "getgraph";
            content.uuid = self.agocontrol.dataLoggerController;
            content.devices = [uuid];
            content.start = start;
            content.end = end;
            self.agocontrol.sendCommand(content, null, 10)
                .then( function(res) {
                    if( !res.result.error && document.getElementById("graphRRD") )
                    {
                        document.getElementById("graphRRD").src = "data:image/png;base64," + res.result.data.graph;
                        $("#graphRRD").show();
                    }
                    else
                    {
                        notif.error('Unable to get graph: '+res.result.msg);
                    }
                })
                .catch(function(err) {
                    notif.error('Unable to get graph: Internal error');
                });
        };
    }

    if( this.devicetype=="barometersensor" )
    {
        if( self.values() && self.values().forecast && typeof self.values().forecast.level==='string' )
        {
            self.forecast = ko.observable(self.values().forecast.level);
        }
        else
        {
            self.forecast = ko.observable(-1);
        }
    }

    if (this.devicetype == "squeezebox")
    {
        this.mediastate = ko.observable(''); //string variable

        this.play = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'play';
            self.agocontrol.sendCommand(content);
        };

        this.pause = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'pause';
            self.agocontrol.sendCommand(content);
        };

        this.stop = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'stop';
            self.agocontrol.sendCommand(content);
        };
    }

    this.allOn = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'allon';
        self.agocontrol.sendCommand(content);
    };

    this.allOff = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'alloff';
        self.agocontrol.sendCommand(content);
    };

    this.turnOn = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'on';
        self.agocontrol.sendCommand(content);
    };

    this.turnOff = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'off';
        self.agocontrol.sendCommand(content);
    };

    this.turnStop = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'stop';
        self.agocontrol.sendCommand(content);
    };

    this.turnPush = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'push';
        self.agocontrol.sendCommand(content);
    };

    this.reset = function()
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'reset';
        self.agocontrol.sendCommand(content);
    };

    this.customCommand = function(params)
    {
        var content = {};
        content.uuid = uuid;
        for ( var key in params) {
            if (params.hasOwnProperty(key)) {
                content[key] = params[key];
            }
        }
        self.agocontrol.sendCommand(content);
    };

    this.execCommand = function()
    {
        var command = document.getElementById("commandSelect").options[document.getElementById("commandSelect").selectedIndex].value;
        var content = {};
        content.uuid = uuid;
        content.command = command;
        var params = document.getElementsByClassName("cmdParam");
        for ( var i = 0; i < params.length; i++)
        {
            content[params[i].name] = params[i].value;
        }
        self.agocontrol.sendCommand(content)
            .then( function(res) {
                notif.info("Done");
            });
    };

    //add device function
    this.addDevice = function(content, callback)
    {
        self.agocontrol.sendCommand(content)
            .then( function(res) {
                if( res.result.error===0 )
                {
                    notif.error(res.result.msg);
                }
                else
                {
                    notif.success(res.result.msg);
                }
                if (callback !== null)
                {
                    callback(res);
                }
            })
            .catch(function(err) {
                notif.fatal('Fatal error: no response received');
            });
    };

    //get devices
    this.getDevices = function(callback)
    {
        var content = {};
        content.uuid = uuid;
        content.command = 'getdevices';
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( callback !== undefined )
                {
                    callback(res);
                }
            });
    };

    if (this.devicetype == "camera")
    {
        this.getVideoFrame = function()
        {
            self.agocontrol.block($('#imageContainer').parent());

            var content = {};
            content.command = "getvideoframe";
            content.uuid = self.uuid;
            // TODO: this had a 90s reply timeout before implementing the promise style - check if this is still needed
            self.agocontrol.sendCommand(content, null, 90)
                .then(function(res) {
                    if( res.data && res.data.image )
                    {
                        $('#imageContainer').attr('src', 'data:image/jpeg;base64,'+res.data.image);
                    }
                })
                .finally(function() {
                    self.agocontrol.unblock($('#imageContainer').parent());
                });
        };

        //camera thumb feature only available on large devices (tablet and desktop)
        self.cameraThumb = ko.observable('');
        if( self.agocontrol.deviceSize=='lg' )
        {
            self.getThumbFrame = function()
            {
                var noFrame = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAC0CAMAAAATtNrjAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAYdEVYdFNvZnR3YXJlAHBhaW50Lm5ldCA0LjAuNvyMY98AAACxUExURQAAAP///wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAMTa1RMAAAA6dFJOUwAABAgMEBQYHCAkKCwwNDg8P0BESExPUFNcX2Bna3B3e3+Hi4+Tl5+rr7e7w8fLz9fb3+Pn6+/z9/tFeqmxAAACWElEQVRo3u3Y21raUBCA0b9yDqZKSlspiqgBQSqYoCWZ93+wXgQRYR9SctmZe9a3DzOT2fClYqCAAgoooIACCiiggAIKKKCAAgoooIACCvw3QJjIskuFeBGR1yqCiEk4+xk/for7bzZgaRK6KzmK55YZ6L4eC421GOKZ0kIsxrgqK7QyM5CclRQexBK/KCU0/tgA6xJ2wvcoiqKJWGMcfUS3ZhD+KbJZWFGQ/Bbgx9XpgtxAa5NXELKABxGH8Jqma6cwIRGbsLq7bADUwtHCCqRFNRqE2df9W2rHluwUxCisLg9TpTN3AgdCHhqy7Tp3AQfCNqub0WDYb78LvY0LOBZa42R7UHGwFXIXcCjc7x1bPq0Xu3ACrnyQpNjT3Ak4heJUOpkTcK+hbux2SGlhCtD2APJkr+48AFh4gDtHf4gBRh6g7+gwKUDoAZquHtUGam7gDWBgO8k+wNoJpABD210MAdISgO02B35gDdC35UPk34LUtsliFJpA3XML4fsqDUICcOEBRruEPxbGAGMPsAAIcpOQtQB+ewBpA0xNlXUPcO4rpiLh64mtTz56gayz350PhTD3AjIvfmNcw/lS/IBcA1CfGk7S3ZV3jaNXlGMQp54eZQFk09t9EvvDYeieQDBOHtf2OaoUIDLvlBWwjR5xu5xwWN77WT0KawDN/t3Tlb1PTpwTzDpN35zfiwlBVm6iMwtZADdSQbgBuM1PFYpBE8JZdoqwP+rWulGJuPh0m5vw5Hea6ZWkQgVhSVWBisJLlYd/dylJqP+hKKCAAgoooIACChzHX+uvTlTRyOtpAAAAAElFTkSuQmCC';
                var content = {};
                content.command = "getvideoframe";
                content.uuid = self.uuid;
                // TODO: this had a 90s reply timeout before implementing the promise style - check if this is still needed
                self.agocontrol.sendCommand(content, null, 90)
                    .then(function(res) {
                        if( res.data && res.data.image )
                        {
                            self.cameraThumb('data:image/jpeg;base64,'+res.data.image);
                        }
                        else
                        {
                            self.cameraThumb(noFrame);
                        }
                    })
                    .catch(function(err) {
                        self.cameraThumb(noFrame);
                    });
            };
    
            //by default get dashboard widget background, but no refresh available
            self.getThumbFrame();
        }
    }

    //update device content
    self.update = function(obj)
    {
        for ( var k in obj)
        {
            if (typeof (this[k]) === "function")
            {
                //should be an observable
                this[k](obj[k]);
            }
            else
            {
                this[k] = obj[k];
            }
        }
    };

    //save device parameters
    self.saveParameters = function()
    {
        //sync device parameters
        for( var i=0; i<self.koparameters().length; i++ )
        {
            var conv = Number(self.koparameters()[i].value());
            if( isNaN(conv) )
            {
                //save string
                self.parameters[self.koparameters()[i].key] = self.koparameters()[i].value();
            }
            else
            {
                //save number
                self.parameters[self.koparameters()[i].key] = conv;
            }
        }

        //save parameters to resolver
        var content = {};
        content.command = 'setdeviceparameters';
        content.uuid = self.agocontrol.agoController;
        content.device = self.uuid;
        content.parameters = self.parameters;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                //close modal
                $('#detailsModal').modal('hide');
            });
    };
}
