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
                var noFrame = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAMAAAD04JH5AAAAM1BMVEUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACjBUbJAAAAEHRSTlMADx8vP09fb3+Pn6+/z9/v+t8hjgAAA69JREFUeNrt28GyqyAMANCAiIgY8v9f+xad+9RWJQlYNmV9ZzhDMZCQC2BCJlo9dBsLERFR7DV/IOorQOosIBIK3N2wVYCywEwrFcYi3c6rQDBmYgx0IoAntiASc8gWIXIFM7GHe0Lg+PMTwgMCFACEPwJL4CXzE5rmAtECEAVoLJAtAFE2jQXCBZAvQUEwSueXL8G9IIkB8iW4Ezj5/IoluBEoFkCzBG+CJfyNWTM/5XA+/M2hHek7A2fbWUAUbW9B9r0F2y4dhk6C6TWjzbmXwP3dCnsJ0Pydc70E4f8510mQt3NOI8CUUkpprRD4bSqZAGe/u/paF5IyHu0SE75gnc7C2Kj54XCfmjEFcbjM3Lz44kSH3JAjmO/zTzEBSCRIQ7ngkCsABUEeObcKu1QAbgWZm/1PFYBbAbuGMmQ9oI3ArnoAW2BcCEtKKYbx87cxqx7AEgzz4XvDOCoFQBqBT2eX4GMiYLMeUBA4vLqHH3diBeBWcBPrcBB/jUByAeOad6gCqwBqQRRuA6AnBaEG0ESANYAW+8BXAdSCQbAEQA8IULAEQE8ItohkKgFvZQ6uYFeeiZUAr8udA7u+VgJYXfaez19E5ADU1g9GboGrAFi0FYzIjYYgOFskAuTWGEHx9MES/N88tgpg1HWkkbkLgbmZF2lECufvckJA2vayNCZG5mfABkijcmoOEAoeAMgETwBEgqUxIEpP59AEsL4HVL6gzWdIH6cqW+CaBKKTgMoVGGaGxj4LUCZA7q0QuAE1yu6Jc6PjOJ1drTiCgbkHi1eybaIsEWxwW3knHE+Xsijw7MSgBNgONZP5AuSn6CXA7koS+BmLY/8CZYC/yPNuayiCzpciAK+KPqxqnsnVgP0STGJBuUJRBuB17bgoYNRoyoBDfioUMC6vDMChSC4ScPoeGIBjfibYByY3AhwTtIH9NbJqtSwADdePMrUvnjzA21uJCbmZgAeg1Xw8EmIbARPwKQCwY4gppTQPNR0QXACt7H7V+AzgbaWbCUDwt9MTAgmAFtteIAK8P8q0EIDwq0HfWADi0IXetBTIWyWJKJ49YlsfNF0g2n6VFHY9ccZNC+piYhR3qx6C06uJJVecC/6Q8dQPqSDzXraeEwQAMNhP8GrAdtRN4OQdF00Fk+CB8wnBLq77/H3BsaXTxm8LPppa7YzfE1y09VrnQ6vhb06mVfHfSLVD2Q30E/wEP8FP8BN8QeB7C1boLYDeAugsQOgsCNBXsEC34ddXI+4/FLdnpiKujp0AAAAASUVORK5CYII=';
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
