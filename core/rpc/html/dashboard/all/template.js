/**
 * Model class
 * @returns {dashBoard}
 */
function dashBoard(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.defaultRow = 4;
    self.defaultCol = 4;
    self.keyword = ko.observable("");
    self.currentPage = ko.observable(1);
    self.deviceList = ko.computed(function()
    {
        //reset current page
        self.currentPage(1);

        var list = self.agocontrol.devices();
        list = list.filter(function(dev)
        {
            if (dev.devicetype == "event" || $.trim(dev.name()) == "")
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
                if (dev.name().toLowerCase().indexOf(keyword) != -1)
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

    self.itemsPerPage = ko.pureComputed(function() {
        if( typeof(Storage)!=='undefined' )
        {
            var row  = localStorage.getItem('dashboardRowSize');
            if( row===null )
            {
                localStorage.setItem('dashboardRowSize', self.defaultRow);
                row = self.defaultRow;
            }
            var col  = localStorage.getItem('dashboardColSize');
            if( col===null )
            {
                localStorage.setItem('dashboardColSize', self.defaultCol);
                col = self.defaultCol;
            }
            out = row * col;
        }
        else
        {
            out = self.defaultRow * self.defaultCol;
        }
        return out;
    });

    self.devicesPerLine = ko.pureComputed(function() {
        if( typeof(Storage)!=='undefined' )
        {
            out  = localStorage.getItem('dashboardColSize');
            if( out===null )
            {
                localStorage.setItem('dashboardColSize', self.defaultCol);
                out = self.defaultCol;
            }
        }
        else
        {
            out = self.defaultCol;
        }
        return out;
    });

    self.devicesPerPage = ko.computed(function()
    {
        var currentList = self.deviceList().chunk(self.itemsPerPage());
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
        var max = Math.ceil(self.deviceList().length / self.itemsPerPage());
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

