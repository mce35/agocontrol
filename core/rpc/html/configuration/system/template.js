/**
 * Model class
 */
function SystemConfig(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.phone = ko.observable('');
    self.email = ko.observable('');
    self.latitude = ko.observable(null);
    self.longitude = ko.observable(null);
    self.map = null;

    //init openlayers
    self.initOL = function()
    {
        //init openlayers
        self.map = new ol.Map({
            layers: [
                new ol.layer.Tile({source: new ol.source.OSM()})
            ],
            view: new ol.View({
                center: [0, 0],
                zoom: 2
            }),
            target: 'myhome'
        });

        self.map.on('singleclick', function(evt) {
            var coordinates = evt.coordinate;
            var wgs84 = ol.proj.transform( coordinates, 'EPSG:3857', 'EPSG:4326');
            self.latitude( roundNumber(wgs84[1],4) );
            self.longitude( roundNumber(wgs84[0],4) );
        });
    };

    //init plugin
    self.init = function()
    {
        //get config values
        self.getParam('system', 'system', 'lat', self.latitude, parseFloat);
        self.getParam('system', 'system', 'lon', self.longitude, parseFloat);
        self.getParam('system', 'system', 'email', self.email, String);
        self.getParam('system', 'system', 'phone', self.phone, String);
    };

    //new tab selected
    self.onTabChanged = function(index, name)
    {
        if( index==1 )
        {
            //workaround to display layer. It seems if div is hidden, layer is not displayed
            self.map.updateSize();

            //center map to coordinates
            if( self.longitude() && self.latitude() )
            {
                var point = [self.longitude(), self.latitude()];
                var center = ol.proj.transform( point, 'EPSG:4326', 'EPSG:3857');
                self.map.getView().setCenter(center);
                self.map.getView().setZoom(17);
            }
        }
    };

    //save position
    self.savePosition = function()
    {
        var res1 = self.setParam('system', 'system', 'lat', self.latitude());
        var res2 = self.setParam('system', 'system', 'lon', self.longitude());
        if( res1 && res2 )
        {
            notif.success('Position saved');
        }
        else
        {
            notif.error('Unable to save position');
        }
    };

    //save contact
    self.saveContact = function()
    {
        var res1 = self.setParam('system', 'system', 'email', self.email());
        var res2 = self.setParam('system', 'system', 'phone', self.phone());
        if( res1 && res2 )
        {
            notif.success('Contact saved');
        }
        else
        {
            notif.error('Unable to save contact');
        }
    };

    //set specified parameter
    self.setParam = function(app, section, option, value)
    {
        var content = {};
        content.uuid = self.agocontrol.agoController;
        content.command = 'setconfig';
        content.app = app;
        content.section = section;
        content.option = option;
        content.value = value;
        result = true;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                result = true;
            })
            .catch(function(err) {
                result = false;
            });
        return result;
    };

    //get specified parameter
    //@param app,section,option : config file option to get
    //@param observable: knockout observable to write value in
    //@param convert: convert function (parseFloat, parseInt, String)
    self.getParam = function(app, section, option, observable, convertFunction)
    {
        var content = {};
        content.uuid = self.agocontrol.agoController;
        content.command = 'getconfig';
        content.app = app;
        content.section = section;
        content.option = option;
        result = true;
        self.agocontrol.sendCommand(content)
            .then(function(res) {
                if( observable )
                {
                    var value = res.data.value;
                    if( value=='' )
                    {
                        observable(null);
                    }
                    else
                    {
                        if( convertFunction )
                        {
                            value = convertFunction(value);
                        }
                        observable(value);
                    }
                }
            })
            .catch(function(err) {
                result = false;
            });
    };

    //view model
    this.openlayersViewModel = new ko.openlayers.viewModel({
        initOL: self.initOL
    });

    //init
    self.init();
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    ko.openlayers = {
        viewModel: function(config) {
            this.initOL = config.initOL;
        }
    };

    ko.bindingHandlers.openlayers = {
        update: function(element, viewmodel) {
            //workaround to make sure openlayers is loaded
            var interval = window.setInterval(function() {
                if( typeof ol!='object' )
                {
                    return;
                }
                window.clearInterval(interval);
                viewmodel().initOL();
            }, 250);
        }
    };

    var model = new SystemConfig(agocontrol);
    return model;
}

