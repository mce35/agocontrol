/**
 * Model class
 * @returns {dashBoard}
 */
function dashBoard(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.itemsPerPage = 9;
    self.itemsPerRow = 3;
    this.keyword = ko.observable("");
    this.deviceList = ko.computed(function()
    {
        var list = self.agocontrol.devices();
        list = list.filter(function(dev)
        {
            if (dev.devicetype == "event" || $.trim(dev.name) == "")
            {
                return false;
            }
            var keyword = self.keyword().toLowerCase();
            if ($.trim(keyword) != "")
            {
                if (dev.devicetype.toLowerCase().indexOf(keyword) != -1)
                {
                    return true;
                }
                if (dev.name.toLowerCase().indexOf(keyword) != -1)
                {
                    return true;
                }
                if (dev.room.toLowerCase().indexOf(keyword) != -1)
                {
                    return true;
                }
                return false;
            }
            return true;
        });
        return list;
    });

    this.page = ko.observable(1);
    this.pages = ko.computed(function()
    {
        var pages = [];
        var max = Math.ceil(self.deviceList().length / self.itemsPerPage);
        for ( var i = 1; i <= max; i++)
        {
            pages.push({idx : i});
        }
        return pages;
    });

    this.firstRow = ko.computed(function()
    {
        var currentList = self.deviceList().chunk(self.itemsPerPage);
        if (currentList.length < self.page())
        {
            return [];
        }
        currentList = currentList[self.page() - 1];
        if (currentList.length >= 0)
        {
            return currentList.chunk(self.itemsPerRow)[0];
        }
        return [];
    });

    this.secondRow = ko.computed(function()
    {
        var currentList = self.deviceList().chunk(self.itemsPerPage);
        if (currentList.length < self.page())
        {
            return [];
        }
        currentList = currentList[self.page() - 1];
        if (currentList.length >= (self.itemsPerRow+1))
        {
            return currentList.chunk(self.itemsPerRow)[1];
        }
        return [];
    });

    this.thirdRow = ko.computed(function()
    {
        var currentList = self.deviceList().chunk(self.itemsPerPage);
        if (currentList.length < self.page())
        {
            return [];
        }
        currentList = currentList[self.page() - 1];
        if (currentList.length >= (self.itemsPerRow*2+1))
        {
            return currentList.chunk(self.itemsPerRow)[2];
        }
        return [];
    });

    //change page
    this.switchPage = function(item)
    {
        self.page(item.idx);
    };

    //clear current search
    self.clearSearch = function()
    {
        console.log('plouf');
        self.keyword("");
    };

    //buildfloorPlanList(this);
    //buildPluginNamesList(this);
}

/**
 * Initalizes the model
 */
function init_template(path, params, agocontrol)
{
    var model = new dashBoard(agocontrol);

    model.deviceTemplate = function(item)
    {
        if (agocontrol.supported_devices().indexOf(item.devicetype) != -1)
        {
            return 'templates/devices/' + item.devicetype;
        }
        return 'templates/devices/empty';
    }.bind(model);

    /*model.mainTemplate = function() {
        return "dashboard";
    }.bind(model);*/

    /*model.navigation = function() {
        return ""; // No navigation
    }.bind(model);*/

    //ko.applyBindings(model);
    return model;
}
