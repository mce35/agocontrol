/**
 * Model class
 * @returns {dashBoard}
 */
function dashBoard(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.itemsPerPage = 12;
    self.keyword = ko.observable("");
    self.currentPage = ko.observable(1);
    self.deviceList = ko.computed(function()
    {
        //reset current page
        self.currentPage(1);

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

    self.devicesPerPage = ko.computed(function()
    {
        var currentList = self.deviceList().chunk(self.itemsPerPage);
        if (currentList.length < self.currentPage())
        {
            return [];
        }
        currentList = currentList[self.currentPage() - 1];
        return currentList;
    });

    self.pages = ko.computed(function()
    {
        var pages = [];
        var max = Math.ceil(self.deviceList().length / self.itemsPerPage);
        for ( var i = 1; i <= max; i++)
        {
            pages.push({idx : i});
        }
        return pages;
    });

    //change page
    self.switchPage = function(item)
    {
        self.currentPage(item.idx);
    };

    //clear current search
    self.clearSearch = function()
    {
        self.keyword("");
    };
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

    return model;
}

