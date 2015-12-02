/**
 * Model class
 */
function Journal(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.journalUuid = null;
    self.types = ko.observableArray(['all', 'debug', 'info', 'warning', 'error']);
    self.type = ko.observable('all');
    self.filter = ko.observable('');
    self.messages = ko.observableArray([]);
    self.startDt = 0;
    self.endDt = 0;

    if( self.agocontrol.devices()!==undefined )
    {
        for( var i=0; i<self.agocontrol.devices().length; i++ )
        {
            if( self.agocontrol.devices()[i].devicetype=='journal' )
            {
                self.journalUuid = self.agocontrol.devices()[i].uuid;
                break;
            }
        }
    }
    
    //get messages from journal
    self.getMessages = function()
    {
        self.agocontrol.block($('#messagesContainer'));

        var content = {};
        content.uuid = self.journalUuid;
        content.command = 'getmessages';
        content.filter = self.filter();
        content.type = self.type();
        content.start = self.startDt.toISOString();
        content.end = self.endDt.toISOString();
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                var prevDay = null;
                var messages = [];
                for( var i=0; i<res.data.messages.length; i++ )
                {
                    var item = {'type':null, 'text':null, 'date':null};
                    var msg = res.data.messages[i];
                    var d = new Date(msg.time*1000);

                    //add new time label
                    if( prevDay!=d.getDate() )
                    {
                        var month = d.getMonth()+1;
                        var day = d.getDate()
                        messages.push({
                            'kind': 'label',
                            'text': dateToString(d)
                        });
                        prevDay = d.getDate();
                    }

                    //add new item
                    messages.push({
                        'kind': 'item',
                        'type': msg.type,
                        'text': msg.message,
                        'time': timeToString(d)
                    });
                }
                messages.push({
                    'kind': 'end'
                });
                self.messages.replaceAll(messages);

            })
            .finally(function() {
                self.agocontrol.unblock($('#messagesContainer'));
            });
    }
};

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new Journal(agocontrol);

    ko.bindingHandlers.dateTimePicker = {
        init : function(element, valueAccessor, allBindingsAccessor)
        {
            //set default date
            if( $(element).attr('id').indexOf('start')!==-1 )
            {
                //start date
                model.startDt = new Date((new Date()).getTime() - 7*24*3600*1000); //now -1 week
                model.startDt.setHours(0);
                model.startDt.setMinutes(0);
                $(element).val(datetimeToString(model.startDt));
            }
            else
            {
                //end date
                model.endDt = new Date();
                model.endDt.setHours(23);
                model.endDt.setMinutes(59);
                $(element).val(datetimeToString(model.endDt));
            }

            $(element).datetimepicker({
                format: getDatetimepickerFormat(),
                onChangeDateTime: function(dt,$input)
                {
                    //check datetime
                    if( $input.attr('id').indexOf('start')!==-1 )
                    {
                        if( dt.getTime()>model.endDt.getTime() )
                        {
                            notif.warning('Specified datetime is invalid');
                            $input.val( datetimeToString(model.startDt) );
                        }
                        else
                        {
                            //update start datetime
                            model.startDt.setTime(dt.getTime());
                        }
                    }
                    else
                    {
                        if( model.startDt.getTime()>dt.getTime() )
                        {
                            notif.warning('Specified datetime is invalid');
                            $input.val( datetimeToString(model.endDt) );
                        }
                        else
                        {
                            //update start datetime
                            model.endDt.setTime(dt.getTime());
                        }
                    }
                }  
            });
        }
    }

    return model;
};

function afterrender_template(model)
{
    if( model )
    {
        model.getMessages();
    }
}

