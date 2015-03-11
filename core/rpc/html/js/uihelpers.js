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

//Init specific knockout bindings
Agocontrol.prototype.initSpecificKnockoutBindings = function()
{
    //dashboard widget slider bindings
    ko.bindingHandlers.slider = {
        init : function(element, valueAccessor, allBindingsAccessor) {
            var options = allBindingsAccessor().sliderOptions || {};
            $(element).slider(options);
            ko.utils.registerEventHandler(element, "slidechange", function(event, ui) {
                var observable = valueAccessor();
                observable(ui.value);
                // Hack to avoid setting the level on startup
                // So we call the syncLevel method when we have
                // a mouse event (means user triggered).
                if (options.dev && event.clientX)
                {
                    options.dev.syncLevel();
                }
            });
            ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
                $(element).slider("destroy");
            });
        },
        update : function(element, valueAccessor) {
            var value = ko.unwrap(valueAccessor());
            if (isNaN(value))
            {
                value = 0;
            }
            $(element).slider("value", value);
        }
    };

    //gumby modal bindings
    ko.bindingHandlers.gumbyModal = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //handle close button
            $('.close.switch').click(function() {
                $(this).parents('.modal').removeClass('active');
                return false; //prevent from bubbling
            });
        }
    };

    //gumby tabs bindings
    ko.bindingHandlers.gumbyTabs = {
        init : function(element, valueAccessor, allBindingsAccessor, viewModel, bindingContext) {
            //handle tab changing
            $('.tab-nav li > a').click(function() {
                $(".tab-nav li").each(function() {
                    $(this).removeClass('active');
                });

                $(this).parent().toggleClass('active');

                this.$el = $('.content');
                var index = $(this).parent().index();
                this.$content = this.$el.find(".tab-content");
                this.$nav = $(this).parent().find('li');
                this.$nav.add(this.$content).removeClass("active");
                this.$nav.eq(index).add(this.$content.eq(index)).addClass("active");
                return false; //prevent from bubbling
            });
        }
    };

};

