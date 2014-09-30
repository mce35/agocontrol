/**
 * ATAlarm plugin
 */
function ATAlarmConfig(deviceMap) {
    //members
    var self = this;
    self.hasNavigation = ko.observable(false);
    self.ATAlarmControllerUuid = null;
    self.port = ko.observable();
    self.wgtags = ko.observableArray();
    self.selectedWGTag = ko.observable();
    self.base_0 = ko.observable();
    self.base_1 = ko.observable();
    self.base_2 = ko.observable();
    self.base_3 = ko.observable();
    self.base_4 = ko.observable();
    self.base_5 = ko.observable();

    //ATAlarm controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='ATAlarmController' )
            {
                self.ATAlarmControllerUuid = deviceMap[i].uuid;
                break;
            }
        }
    }

    //set port
    self.setPort = function() {
        //first of all unfocus element to allow observable to save its value
        $('#setport').blur();
        var content = {
            uuid: self.ATAlarmControllerUuid,
            command: 'setport',
            port: self.port()
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                if( res.result.error==0 )
                {
                    notif.success('#sp');
                }
                else
                {
                    //error occured
                    notif.error(res.result.msg);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get port
    self.getPort = function() {
        var content = {
            uuid: self.ATAlarmControllerUuid,
            command: 'getport'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                self.port(res.result.port);
            }
	    else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //reset counters
    self.resetCounters = function() {
        if( confirm("Reset all counters?") )
        {
            var content = {
                uuid: self.ATAlarmControllerUuid,
                command: 'resetcounters'
            }
    
            sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    notif.success('#rc');
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //get devices list
    self.getWGTags = function() {
        var content = {
            uuid: self.ATAlarmControllerUuid,
            command: 'getwgtags'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
		//alert(JSON.stringify(res.result.wgtags));
                self.wgtags(res.result.wgtags);
		self.freetag = res.result.freetag;
		if(self.freetag >= 0)
		{
		    $("#newtag").prop('disabled', false);
		    $("#addtagbtn").removeClass('default');
		    $("#addtagbtn").addClass('primary');
		}
		else
		{
		    $("#newtag").prop('disabled', true);
		    $("#addtagbtn").removeClass('primary');
		    $("#addtagbtn").addClass('default');
		}
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //remove wg tag
    self.removeWGTag = function() {
        if( confirm('Delete tag ' + self.selectedWGTag().tag + ' ?'))
        {
            var content = {
                uuid: self.ATAlarmControllerUuid,
                command: 'setwgtag',
                wgtagpos: self.selectedWGTag().pos,
		wgtag: 0
            }

            sendCommand(content, function(res)
            {
                if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
                {
                    if( res.result.error==0 )
                    {
                        notif.success('#msg_tag_rmok');
                        //refresh devices list
                        self.getWGTags();
                    }
                    else 
                    {
                        //error occured
                        notif.error(res.result.msg);
                    }
                }
                else
                {
                    notif.fatal('#nr', 0);
                }
            });
        }
    };

    //set wg tag
    self.setWGTag = function() {
        var content = {
            uuid: self.ATAlarmControllerUuid,
            command: 'setwgtag',
            wgtag: $('#newtag').val(),
	    wgtagpos: self.freetag
        }

	if(self.freetag < 0)
	{
	    alert("Cannot add more tag, you need to remove at least one tag before adding a new one.");
	}
	else
	{
            sendCommand(content, function(res)
                {
		    if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
		    {
			if( res.result.error==0 )
			{
			    notif.success('#msg_tagok');
			    //refresh devices list
			    self.getWGTags();
			    $('#newtag').val('');
			}
			else 
			{
			    //error occured
			    notif.error(res.result.msg);
			}
		    }
		    else
		    {
			notif.fatal('#nr', 0);
		    }
		});
	}
    };

    self.setBase = function(contact) {
    };

    //init ui
    $('#atconf').prop('disabled', true);
    self.getPort();
    self.getWGTags();
}

function ATAlarmDashboard(deviceMap) {
    //members
    var self = this;
    self.hasNavigation = ko.observable(false);
    self.ATAlarmControllerUuid = null;
    self.port = ko.observable();
    self.counters = ko.observableArray([]);

    //ATAlarm controller uuid
    if( deviceMap!==undefined )
    {
        for( var i=0; i<deviceMap.length; i++ )
        {
            if( deviceMap[i].devicetype=='ATAlarmController' )
            {
                self.ATAlarmControllerUuid = deviceMap[i].uuid;
                break;
            }
        }
    }

    //get counters
    self.getCounters = function() {
        var content = {
            uuid: self.ATAlarmControllerUuid,
            command: 'getcounters'
        }

        sendCommand(content, function(res)
        {
            if( res!==undefined && res.result!==undefined && res.result!=='no-reply')
            {
                for( device in res.result.counters )
                {
                    self.counters.push(res.result.counters[device]);
                }
            }
            else
            {
                notif.fatal('#nr', 0);
            }
        });
    };

    //get counters
    self.getCounters();
}

/**
 * Entry point: mandatory!
 */
function init_plugin(fromDashboard)
{
    var model;
    var template;
    if( fromDashboard )
    {
        model = new ATAlarmDashboard(deviceMap);
        template = 'ATAlarmDashboard';
    }
    else
    {
        model = new ATAlarmConfig(deviceMap);
        template = 'ATAlarmConfig';
    }
    model.mainTemplate = function() {
        return templatePath + template;
    }.bind(model);
    ko.applyBindings(model);
}
