/**
 * The plugin model
 * @returns {examplePlugin}
 */
function examplePlugin() {
    this.exampleText = ko.observable("Hello World");
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    //DEFAULT BEHAVIOUR
    model = new examplePlugin();
    /*model.mainTemplate = function() {
        return templatePath + "templates/example";
    }.bind(model);*/
    model.mainTemplate = path + "templates/example";

    //DASHBOARD AND CONFIG PAGES
    //you can make a config page and a different dashboard page using input parameter fromDashboard
    //if true plugin is loaded from dashboard, otherwise it's loaded from configuration
    //if( fromDashboard ) {
    //    model = new exampleDashboardPlugin();
    //    template = templatePath + "exampleDashboard";
    //}
    //else {
    //    model = new exampleConfigurationPluagin();
    //    template = templatePath + "exampleConfiguration";
    //}
    //model.mainTemplate = function() {
    //    return template;
    //}.bind(model);

    //MENU
    //By default configuration menu is applied to all plugins
    //but if you want to add specific menu to your plugin do that:
    //model.hasNavigation = ko.observable(true);
    //model.navigation = function() {
    //    return templatePath + "navigation/<mytemplate without .html>";
    //}.bind(model);
    //
    //to remove my and default menu
    //model.hasNavigation = ko.observable(false);

    return model;
}
