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
        var content = {};
        content.uuid = self.journalUuid;
        content.command = 'getmessages';
        content.filter = self.filter();
        content.type = self.type();
        content.start = stringToDatetime($('#journal_start').val());
        content.end = stringToDatetime($('#journal_end').val());
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                self.agocontrol.block($('#messagesContainer'));

                //clear container
                $('#messagesContainer > ul').empty();

                //inject all messages to container
                for( var i=0; i<res.data.messages.length; i++ )
                {
                    var msg = res.data.messages[i];
                    var type = "default";
                    if( msg.type=="debug" )
                    {
                        type = "primary";
                    }
                    else if( type=="info" )
                    {
                        type = "default";
                    }
                    else if( type=="warning" )
                    {
                        type = "warning"
                    }
                    else if( type=="error" )
                    {
                        type = "danger"
                    }
                    var d = datetimeToString(new Date(msg.time*1000));
                    $('#messagesContainer > ul').append('<li class="'+type+' alert">'+d+' : '+msg.message+'</i>');
                }

                //scroll to end of container
                $('#messagesContainer > ul').scrollTop($('#messagesContainer > ul')[0].scrollHeight);

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
                $(element).val($.datepicker.formatDate('dd/mm/yy', start) + ' 00:00');
            }
            else
            {
                //end date
                $(element).val($.datepicker.formatDate('dd/mm/yy', new Date())+' 23:59');
            }

            $(element).datetimepicker({
                format: 'd/m/Y H:i',
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

