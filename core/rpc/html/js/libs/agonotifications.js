// IIFE - Immediately Invoked Function Expression
(function(yourcode) {

    // The global jQuery object is passed as a parameter
    yourcode(window.jQuery, window, document);

} (function($, window, document) {
    // The $ is now locally scoped 

    // Listen for the jQuery ready event on the document
    $(function() {
        // The DOM is ready!
        notif = new agoNotifications();
        notif.init();
    });

    //The DOM may not be ready
    var NOTIF_INFO = 0;
    var NOTIF_SUCC = 1;
    var NOTIF_WARN = 2;
    var NOTIF_ERRO = 3;
    var NOTIF_FATA = 4;

    function agoNotifications() {
        //members
        var self = this;
        self.name = "notification-box";
        self.duration = 5; //seconds

        //create main container
        self.init = function() {
            self.container = $("<div></div>");
            self.container.css({
                "width":"20%",
                "bottom":"0px",
                "right":"0px",
                "z-index":"1000000",
                "position":"fixed",
                "margin-right":"10px"
            }).attr("name", self.name);
            self.container.appendTo($(document.body));
        };

        //notify message (internal use)
        self._notify = function(message, type, duration) {
            if( self.container!==undefined )
            {
                if( typeof duration=='undefined' )
                    duration = self.duration;
                if( typeof message=='string' && message.charAt(0)=="#" )
                    message = $(message).html();
                var msg = new agoNotificationMessage();
                msg.init(self.container, message, type, duration);
                msg.show();
            }
            else
            {
                console.log('container is undefined');
            }
        };

        //notify info message
        self.info = function(message, duration) {
            self._notify(message, NOTIF_INFO, duration);
        };

        //notify success message
        self.success = function(message, duration) {
            self._notify(message, NOTIF_SUCC, duration);
        };

        //notify warning message
        self.warning = function(message, duration) {
            self._notify(message, NOTIF_WARN, duration);
        };

        //notify error message
        self.error = function(message, duration) {
            self._notify(message, NOTIF_ERRO, duration);
        };

        //notify fatal message
        self.fatal = function(message) {
            self._notify(message, NOTIF_FATA, 0);
        };
    };

    function agoNotificationMessage() {
        //members
        var self = this;

        //init
        self.init = function(container, message, type, duration) {
            self.container = container;
            self.message = message;
            self.type = type;
            self.duration = duration*1000;
        };

        //show notification
        self.show = function() {
            //build elem
            self.elem = $("<div></div>").css({
                "width":"100%",
                "cursor":"pointer"
            });
            var icon;
            if( self.type==NOTIF_SUCC )
            {
                self.elem.attr("class", "row agonotif agonotif-success");
                icon = "en-thumbs-up";
            }
            else if( self.type==NOTIF_ERRO )
            {
                self.elem.attr("class", "row agonotif agonotif-error");
                icon = "en-alert";
            }
            else if( self.type==NOTIF_WARN )
            {
                self.elem.attr("class", "row agonotif agonotif-warning");
                icon = "en-attention";
            }
            else if( self.type==NOTIF_FATA )
            {
                self.elem.attr("class", "row agonotif agonotif-fatal");
                icon = "en-thumbs-down";
            }
            else
            {
                self.elem.attr("class", "row agonotif agonotif-info");
                icon = "en-info-circled";
            }
            //icon
            self.icon = $("<div></div>").attr("class", "col-sm-2").css("padding-left", "0px");
            self.icon.html('<span class="'+icon+'" style="font-size:200%;"></span>');
            //message
            self.msg = $("<div></div>").attr("class", "col-sm-10").css("padding-left", "0px");
            self.msg.html(self.message);
            //append to notification container
            self.elem.append(self.icon);
            self.elem.append(self.msg);
            self.container.append(self.elem);
            //close event
            self.elem.click(function() { self.hide(); });
            //autoclose after duration
            if( self.duration>0 )
            {
                self.elem.delay(self.duration).slideUp(300, function() {
                    $(this).remove();
                });
            }
        };

        //hide notification
        self.hide = function() {
            if( self.elem!==undefined )
                self.elem.remove();
        };
    };

}));
