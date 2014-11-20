/**
 * Append UI helpers to Agocontrol object
 */

//Block specified DOM element
Agocontrol.prototype.block = function(el)
{
    $(el).block({ message: '<img src="img/loading.gif" />' });
};

//Unblock specified DOM element
Agocontrol.prototype.unblock = function(el)
{
    $(el).unblock();    
};

//Avoid datatables's previous/next click to change current page
//must be called after model is rendered, so call it in your model afterRender function
Agocontrol.prototype.stopDatatableLinksPropagation = function(datatableId)
{
    var next = $('#' + datatableId + '_next');
    var prev = $('#' + datatableId + '_previous');
    if( next[0] && prev[0] )
    {
        next.click(function() {
            return false;
        });
        prev.click(function() {
            return false;
        });
    }
};

