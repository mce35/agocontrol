/**
 * agodrain by jaeger@agocontrol.com
 * @returns {agodrainPlugin}
 */
function agodrainPlugin(agocontrol)
{
    this.rooms = ko.observableArray([]);

    var self = this;
    //var _originalHandler = handleEvent;
    var _originalHandler = agocontrol.handleEvent;
    var subDisplayed = false;

    this.displayEvent = function(requestSucceed, response) {
        if (!requestSucceed) {
            return;
        } else {
            if (response.result) { 
                var output = document.getElementById('output');
                var content = JSON.stringify(response.result);
                output.innerHTML = '<li class="default alert">' + content + '</li>' + output.innerHTML;
            }
            getEvent();
        }
    };

    this.stopDrain = function() {
        agocontrol.handleEvent = _originalHandler;
    };

    this.clear = function() {
        var output = $('#output');
        output.empty();
    }

    this.startDrain = function() {
        if (!self.subDisplayed) {
            var output = document.getElementById('output');
            output.innerHTML = '<br>';
            output.innerHTML = output.innerHTML + '<li class="success alert">Client subscription uuid: ' + subscription + '</li>';
            self.subDisplayed = true;
        }
        _originalHandler = handleEvent;
        handleEvent = self.displayEvent;
    };
}

function init_template(path, params, agocontrol)
{
    model = new agodrainPlugin(agocontrol);
    /*model.mainTemplate = function() {
        return templatePath + "agodrain";
    }.bind(model);*/

    return model;
}
