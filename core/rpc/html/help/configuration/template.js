/**
 * Model class
 * 
 * @returns {configuration}
 */
function configuration() {
    this.hasNavigation = ko.observable(true);


}

/**
 * Initalizes the Configuration model
 */
function init_template(path, params, agocontrol)
{
    ko.bindingHandlers.initTabs = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            $('.tab-nav li > a').click(function()
            {
                $(".tab-nav li").each(function()
                {
                    $(this).removeClass('active');
                });

                $(this).parent().toggleClass('active');

                this.$el = $('.tabs');
                var index = $(this).parent().index();
                this.$content = this.$el.find(".tab-content");
                this.$nav = $(this).parent().find('li');
                this.$nav.add(this.$content).removeClass("active");
                this.$nav.eq(index).add(this.$content.eq(index)).addClass("active");
                return false; //prevent from bubbling
            });
        },
        update : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext)
        {
             // NOTHING
        }
    };

    var model = new configuration();

    return model;
}
