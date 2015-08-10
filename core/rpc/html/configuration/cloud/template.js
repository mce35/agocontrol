/**
 * Model class
 * 
 * @returns {CloudConfig}
 */
function CloudConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    this.cloudURL = ko.observable("");

    this.cloudUsername = ko.observable("");
    this.cloudPassword = ko.observable("");
    this.cloudPasswordConfirm = ko.observable("");
    this.cloudPIN = ko.observable("");

    this.checkPassWords = function() {
        if (self.cloudPassword() != self.cloudPasswordConfirm())
        {
            notif.error("#passwordError");
            return false;
        }
        return true;
    };

    this.cloudURL("https://cloud.agocontrol.com/cloudreg/" + self.agocontrol.system().uuid + "/");

    //Cloud Activation
    self.cloudActivate = function()
    {
        var cloudUsername = this.cloudUsername();
        var cloudPassword = this.cloudPassword();
        var cloudPIN = this.cloudPIN();
        var buttons = {};
        var closeButton = $("#closeButton").data("close");

        buttons[closeButton] = function()
        {
            $(self.openDialog).dialog("close");
            self.openDialog = null;
        };

        var url = "cgi-bin/activate.cgi?action=activate&username=" + cloudUsername + "&password=" + cloudPassword + "&pin=" + cloudPIN;
        $.ajax({
            type : 'POST',
            url : url,
            success : function(res) {
                var result = JSON.parse(res);
                if( result.rc===0 )
                {
                    notif.success('#cloudActivationResult_0');
                else
                {
                    notif.error('#cloudActivationResult_'+result.rc);
                }
            },
            async : true
        });
    };
}

/**
 * Initalizes the cloudConfig model
 */
function init_template(path, params, agocontrol)
{
    var model = new CloudConfig(agocontrol);
    return model;
}
