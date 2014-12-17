/**
 * List of available plugins
 */
function Plugins(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.favoritesMax = ko.observable(10); //11 items max on gumby menu
    self.favoritesCount = ko.observable(0);
    
    //filter dashboard that don't need to be displayed
    //need to do that because datatable odd is broken when filtering items using knockout
    self.plugins = ko.computed(function() {
        var plugins = [];
        for( var i=0; i<self.agocontrol.plugins().length; i++ )
        {
            if( self.agocontrol.plugins()[i].listable )
            {
                plugins.push(self.agocontrol.plugins()[i]);
            }
        }
        return plugins;
    });

    self.initFavorites = ko.computed(function() {
        var favCount = 0;
        for( var i=0; i<self.agocontrol.plugins().length; i++ )
        {
            if( self.agocontrol.plugins()[i].listable && self.agocontrol.plugins()[i].favorite===true )
            {
                favCount++;
            }
        }
        self.favoritesCount(favCount);
    });

    self.grid = new ko.agoGrid.viewModel({
        data: self.plugins,
        columns: [
            {headerText:'Name', rowText:'name'},
            {headerText:'Version', rowText:'version'},
            {headerText:'Description', rowText:'description'},
            {headerText:'Favorite', rowText:''}
        ],
        rowTemplate: 'rowTemplate',
        displaySearch: false,
        displayPagination: false,
        displayRowCount: false
    });

    self.favoriteClick = function(item)
    {
        var state = !item.fav();

        //check favorites count
        if( state && self.favoritesCount()>=self.favoritesMax() )
        {
            notif.warning('Max favorites count reached (max:'+self.favoritesMax()+')');
            return;
        }

        //update ui
        item.favorite = state;
        item.fav(state);
        if( item.fav() )
            self.favoritesCount(self.favoritesCount()+1);
        else
            self.favoritesCount(self.favoritesCount()-1);

        //send changes
        $.ajax({
            url : "cgi-bin/ui.cgi?key="+item.dir+"&param=favorites&value="+state,
            method : "GET",
            async : false,
        }).done(function(result) {
            if( !result || !result.result || result.result===0 )
            {
                //revert changes
                item.fav(!state);
                item.favorite = !state;
                if( item.fav() )
                    self.favoritesCount(self.favoritesCount()+1);
                else
                    self.favoritesCount(self.favoritesCount()-1);

                //and log error
                notif.error('Unable to change favorite state');
                if( result.error && result.error.length>0 )
                {
                    console.error(result.error);
                }
            }
        });
    };

}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new Plugins(agocontrol);
    return model;
}

