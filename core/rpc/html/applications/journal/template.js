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
        content.start = stringToDatetime($('#journal_start').val());
        content.end = stringToDatetime($('#journal_end').val());
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
    var start = new Date((new Date()).getTime() - 7*24*3600*1000); //now -1 week
    var end = new Date((new Date()).getTime()); 

    ko.bindingHandlers.dateTimePicker = {
        init : function(element, valueAccessor, allBindingsAccessor)
        {
            //set default date
            var id = $(element).attr('id');
            if( id.indexOf('start')!==-1 )
            {
                //start date
                var start = new Date((new Date()).getTime() - 7*24*3600*1000); //now -1 week
                start.setHours(0);
                start.setMinutes(0);
                $(element).val(datetimeToString(start));
            }
            else
            {
                //end date
                var end = new Date();
                end.setHours(23);
                end.setMinutes(59);
                $(element).val(datetimeToString(end));
            }

            $(element).datetimepicker({
                format: getDatetimepickerFormat(),
                onChangeDateTime: function(dp,$input)
                {
                    //check date
                    var sd = stringToDatetime($('#journal_start').val());
                    var ed = stringToDatetime($('#journal_end').val());
                    if( sd.getTime()>ed.getTime() )
                    {
                        //invalid date
                        notif.warning('Specified datetime is invalid');
                        sd = new Date(ed.getTime() - 24 * 3600 * 1000);
                        $(element).val( datetimeToString(sd) );
                    }
                }  
            });
        }
    }

    var model = new Journal(agocontrol);
    return model;
};

function afterrender_template(model)
{
    if( model )
    {
        model.getMessages();
    }
}

