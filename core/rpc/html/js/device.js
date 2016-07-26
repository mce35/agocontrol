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
    this.timeStamp = ko.observable(timestampToString(this.lastseen));

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
                deferred.observable('data:image/png;base64,' + res.data.graph);
            })
            .catch(function(err) {
                //no thumb available
                console.error('Request getthumb failed', err);
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

                if( self.values()[k].level!==null && self.values()[k].level!==undefined )
                {
                    result.push({
                        name : k.charAt(0).toUpperCase() + k.substr(1),
                        level : self.values()[k].level,
                        unit : unit,
                        levelUnit : ''+self.values()[k].level + (unit? ' '+unit:'')
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

    if (this.devicetype == "squeezebox" || this.devicetype == "mopidy")
    {
        this.mediastate = ko.observable(''); //string variable
        this.title = ko.observable('Unknown');
        this.album = ko.observable('Unknown');
        this.artist = ko.observable('Unknown');
        this.cover = ko.observable('data:image/png;base64,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');

        this.updateMediaInfos = function(infos)
        {
            //update cover first only if album is different
            if( infos.cover && this.album()!=infos.album )
            {
                this.cover('data:image/jpeg;base64,'+infos.cover);
            }

            //update other infos
            if( infos.title && infos.album && infos.artist )
            {
                this.title(infos.title.substring(0,55));
                this.album(infos.album);
                this.artist(infos.artist);
            }
        };

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

        this.previous = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'previous';
            self.agocontrol.sendCommand(content);
        };

        this.next = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'next';
            self.agocontrol.sendCommand(content);
        };

        //request media infos
        this.requestMediaInfos = function()
        {
            var content = {};
            content.uuid = uuid;
            content.command = 'mediainfos';
            self.agocontrol.sendCommand(content)
                .then(function(resp) {
                    self.updateMediaInfos(resp);
                });
        };
        this.requestMediaInfos();
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


    // If we have simple commands, add generic execCommand
    var schematype;
    var cmds;
    if( (schematype = self.agocontrol.schema().devicetypes[self.devicetype]) &&
        (cmds = schematype.commands) && cmds.length ) {
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
    }
    else
        this.execCommand = null;

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
