/**
 * List of available applications
 */
function Applications(agocontrol)
{
    var self = this;
    self.agocontrol = agocontrol;
    self.favoritesMax = ko.observable(10); //11 items max on gumby menu
    self.favoritesCount = ko.observable(0);
    
    //filter applications that don't need to be displayed
    self.applications = ko.computed(function() {
        var applications = [];
        var raw = self.agocontrol.applications();
        for( var i=0; i<raw.length; i++ )
        {
            if( raw[i].listable )
            {
                applications.push(raw[i]);
            }
        }
        return applications;
    });

    self.initFavorites = ko.computed(function() {
        var favCount = 0;
        var raw = self.agocontrol.applications();
        for( var i=0; i<raw.length; i++ )
        {
            if( raw[i].listable && raw[i].favorite===true )
            {
                favCount++;
            }
        }
        self.favoritesCount(favCount);
    });

    self.grid = new ko.agoGrid.viewModel({
        data: self.applications,
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

    self.favoriteClick = function(event, state, item, el)
    {
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
            async : true,
        }).done(function(result) {
            if( !result || !result.result || result.result===0 )
            {
                //revert changes
                $(el).bootstrapSwitch('toggleState', true);
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
    var model = new Applications(agocontrol);
    return model;
}

