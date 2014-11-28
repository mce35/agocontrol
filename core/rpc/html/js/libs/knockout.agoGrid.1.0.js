/**
 * agoGrid is pure knockout gumbyfied grid
 * agoGrid is based on simpleGrid from knockoutjs website (http://knockoutjs.com/examples/grid.html)
 *
 * Benefits over jquery-datatables:
 *  - lighter (nearly the same size than bindings file)
 *  - supported (jquery-datatables is maintained but not knockout.bindings.dataTables.js)
 *  - no language pack needed
 *  - fits to agocontrol needs
 *  - pure knockout
 *  - gumbified
 *
 * agoGrid supports:
 * - sorting by columns (asc and desc)
 * - searching data
 * - pagination
 * - changing displayed rows
 * - jquery-jeditable compatible
 * - knockout templating
 * - strict filters
 *
 * Configuration:
 *  - data: data to display in grid (array) [MANDATORY].
 *  - columns: array of column names and column mappings.
 *             Format:
 *              - headerText : displayed column name [MANDATORY]
 *              - rowText: name of real column name in data item [MANDATORY but can be '']
 *             Sample:
 *              columns: [{headerText:'President', rowText:'name'}, {headerText:'Took office', rowText:'took'}, {headerText:'Left office', rowText:'left'}]
 *              data: [{name:'George Washington', took:'April 30, 1789', left:'March 4, 1797'}, {name:'John Adams', took:'March 4, 1797', left:'March 4, 1801'}] 
 *  - rowCallback: callback when row is clicked. Useful for jquery-jeditable.
 *  - rowTemplate: specify custom knockout template name (http://knockoutjs.com/documentation/template-binding.html).
 *  - pageSize: change page size at startup (can be 10, 25, 50 or 100) [DEFAULT=10].
 *  - header: display header [DEFAULT=true]
 *  - footer: display footer [DEFAULT=true]
 *
 * Usage:
 *  1 - Declare new agoGrid (called myGrid) in your knockout code and configure it.
 *  2 - Create new div in your html code with agoGrid bindings: <div data-bind="agoGrid:myGrid"/>
 *  3 - If you want to use filters, add filter using myGrid.addFilter('acolumn', 'avalue') as much as you need.
 *      And call myGrid.resetFilters() to clear all filters.
 */

(function () {
    // Private function
    function getColumnsForScaffolding(data) {
        if ((typeof data.length !== 'number') || data.length === 0) {
            return [];
        }
        var columns = [];
        for (var propertyName in data[0]) {
            columns.push({ headerText: propertyName, rowText: propertyName });
        }
        return columns;
    }

    ko.agoGrid = {
        // Defines a view model class you can use to populate a grid
        viewModel: function (configuration) {
            this.allData = configuration.data;
            this.data = configuration.data;
            this.header = configuration.header===undefined ? true : configuration.header;
            this.footer = configuration.footer===undefined ? true : configuration.footer;
            this.rowCallback = configuration.rowCallback;
            this.currentPageIndex = ko.observable(0);
            this.pageSize = ko.observable(configuration.pageSize || 10);
            this.pageSizes = ko.observableArray([10, 25, 50, 100]);
            this.hasPreviousPage = ko.observable(false);
            this.hasNextPage = ko.observable(false);
            this.search = ko.observable('');
            this.filters = ko.observableArray([]);
            this.sortKey = ko.observable('');
            this.sortAsc = ko.observable(true);
            // If you don't specify columns configuration, we'll use scaffolding
            this.columns = configuration.columns || getColumnsForScaffolding(ko.unwrap(this.data));
            this.rowTemplate = configuration.rowTemplate || "ko_agoGrid_row";
            this.bodyTemplate = configuration.rowTemplate ? "ko_agoGrid_customBody" : "ko_agoGrid_body";

            this.data = ko.computed(function() {
                var out = [];

                //apply filters
                if( this.filters().length>0 )
                {
                    for( var i=0; i<this.filters().length; i++ )
                    {
                        for( var j=0; j<this.allData().length; j++ )
                        {
                            if( this.allData()[j][this.filters()[i].column]!==undefined && this.allData()[j][this.filters()[i].column]===this.filters()[i].value )
                            {
                                out.push(this.allData()[j]);
                            }
                        }
                    }
                }
                else
                {
                    //no filter, copy all data
                    out = this.allData().slice();
                }

                //apply search
                if( $.trim(this.search()).length>0 )
                {
                    for( var i=out.length-1; i>=0; i-- )
                    {
                        var found = false;
                        for( var j=0; j<this.columns.length; j++ )
                        {
                            if( this.columns[j].rowText && this.columns[j].rowText.length>0 )
                            {
                                if( (''+out[i][this.columns[j].rowText]).toLowerCase().indexOf(this.search())>=0 )
                                {
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if( !found )
                        {
                            out.splice(i, 1);
                        }
                    }
                }

                //apply sort
                if( this.sortKey().length>0 )
                {
                    var key = this.sortKey();
                    var asc = this.sortAsc();
                    out = out.sort(function(a,b) {
                        if( asc )
                        {
                            if( ko.utils.unwrapObservable(a[key]).toLowerCase() < ko.utils.unwrapObservable(b[key]).toLowerCase() )
                                return -1;
                            else if( ko.utils.unwrapObservable(a[key]).toLowerCase() > ko.utils.unwrapObservable(b[key]).toLowerCase() )
                                return 1;
                            return 0;
                        }
                        else
                        {
                            if( ko.utils.unwrapObservable(a[key]).toLowerCase() > ko.utils.unwrapObservable(b[key]).toLowerCase() )
                                return -1;
                            else if( ko.utils.unwrapObservable(a[key]).toLowerCase() < ko.utils.unwrapObservable(b[key]).toLowerCase() )
                                return 1;
                            return 0;
                        }
                    });
                }

                return out;
            }, this);

            this.rowCount = ko.computed(function() {
                return this.data().length;
            }, this);

            this.itemsOnCurrentPage = ko.computed(function () {
                var startIndex = this.pageSize() * this.currentPageIndex();
                return ko.unwrap(this.data).slice(startIndex, startIndex + this.pageSize());
            }, this);

            this.maxPageIndex = ko.computed(function () {
                this.currentPageIndex(0);
                return Math.ceil(ko.unwrap(this.data).length / this.pageSize()) - 1;
            }, this);

            this.rangeMin = ko.computed(function() {
                return this.pageSize() * this.currentPageIndex() + 1;
            }, this);

            this.rangeMax = ko.computed(function() {
                return this.pageSize() * this.currentPageIndex() + this.itemsOnCurrentPage().length;
            }, this);

            this.range = ko.computed(function() {
                return this.rangeMin()+'-'+this.rangeMax()+'/'+this.rowCount();
            }, this);

            this.gotoPreviousPage = function() {
                if( this.hasPreviousPage() )
                {
                    this.currentPageIndex(this.currentPageIndex()-1);
                }
            };

            this.gotoNextPage = function() {
                if( this.hasNextPage() )
                {
                    this.currentPageIndex(this.currentPageIndex()+1);
                }
            };

            this.clearSearch = function() {
                this.search('');
            };

            this.updatePagination = ko.computed(function() {
                if( this.currentPageIndex()==0 )
                {
                    this.hasPreviousPage(false);
                }
                else
                {
                    this.hasPreviousPage(true);
                }
                if( this.currentPageIndex()+1<=this.maxPageIndex() )
                {
                    this.hasNextPage(true);
                }
                else
                {
                    this.hasNextPage(false);
                }
            }, this);
    
            this.sortBy = function(view, index, key) {
                if( key && key.length>0 )
                {
                    if( key!=view.sortKey() )
                    {
                        view.sortAsc(true);
                        view.sortKey(key);
                    }
                    else
                    {
                        view.sortAsc(!view.sortAsc());
                    }
                }
            };

            this.onCellClick = function(view, item, evt) {
                if( view.rowCallback )
                {
                    //callback with parameters: clicked item, clicked cell (td), clicked row (tr)
                    view.rowCallback(item, evt.target, evt.target.parentElement);
                }
            };

            this.addFilter = function(column, value) {
                this.filters.push({column:column, value:value});
            };

            this.resetFilters = function() {
                this.filters.removeAll();
            };
        }
    };

    // Templates used to render the grid
    var templateEngine = new ko.nativeTemplateEngine();

    templateEngine.addTemplate = function(templateName, templateMarkup) {
        document.write("<script type='text/html' id='" + templateName + "'>" + templateMarkup + "<" + "/script>");
    };

    //default header template
    templateEngine.addTemplate("ko_agoGrid_header", "\
                <!-- ko if:header -->\
                <div class=\"row\">\
                    <div class=\"six columns\">\
                        <div class=\"picker\"> \
                            <select data-bind=\"options:pageSizes, value:pageSize\"></select>\
                        </div>\
                    </div>\
                    <div class=\"six columns\" style=\"text-align:right;\">\
                        <div class=\"prepend append field\">\
                            <span class=\"adjoined\"><i class=\"icon-search\"></i></span>\
                            <input class=\"normal text input\" type=\"text\" data-bind=\"textInput:search\" />\
                            <div class=\"medium primary btn\"><a href=\"#\" data-bind=\"click:clearSearch\">x</a></div>\
                        </div>\
                    </div>\
                </div>\
                <!-- /ko -->");

    //default grid template
    templateEngine.addTemplate("ko_agoGrid_grid", "\
                    <table class=\"rounded striped\" style=\"margin-top:0px; margin-bottom:0px;\"> \
                        <thead>\
                            <tr data-bind=\"foreach: columns\">\
                               <th>\
                                  <!-- ko ifnot: rowText=='' -->\
                                      <span style=\"cursor:pointer;\" data-bind=\"text:headerText, click:$parent.sortBy.bind($data, $parent, $index, rowText)\"></span>\
                                  <!-- /ko -->\
                                  <!-- ko if: rowText=='' -->\
                                      <span data-bind=\"text:headerText\"></span>\
                                  <!-- /ko -->\
                               </th>\
                            </tr>\
                        </thead>\
                        <tbody data-bind=\"template : { name:bodyTemplate, foreach:itemsOnCurrentPage }\">\
                        </tbody>\
                    </table>");

    //default body template
    templateEngine.addTemplate("ko_agoGrid_body", "\
                            <tr data-bind=\"foreach:$parent.columns, click:$parent.onCellClick.bind($data, $parent)\">\
                               <td data-bind=\"text: typeof rowText == 'function' ? rowText($parent) : $parent[rowText]\"></td>\
                            </tr>");

    //template used when user provide custom row template
    templateEngine.addTemplate("ko_agoGrid_customBody", "\
                            <tr data-bind=\"template: { name:$parent.rowTemplate }, click:$parent.onCellClick.bind($data, $parent)\">\
                            </tr>");

    //default footer template
    templateEngine.addTemplate("ko_agoGrid_footer", "\
                <!-- ko if:footer -->\
                    <div class=\"row\">\
                        <div class=\"four columns\">\
                            <span style=\"font-weight:bold;\" data-bind=\"text:range\"></span>\
                        </div>\
                        <div class=\"eight columns\" style=\"text-align:right;\">\
                            <div class=\"small oval default btn\"><a href=\"#\" data-bind=\"click:gotoPreviousPage\"><<</a></div>\
                            <!-- ko foreach: ko.utils.range(0, maxPageIndex) -->\
                                <!-- ko if:$data == $root.currentPageIndex() -->\
                                    <div class=\"small oval primary btn\"><a href=\"#\" data-bind=\"text:$data+1, click:function() { $root.currentPageIndex($data) }\"></a></div>\
                                <!-- /ko -->\
                                <!-- ko ifnot:$data == $root.currentPageIndex() -->\
                                    <div class=\"small oval default btn\"><a href=\"#\" data-bind=\"text:$data+1, click:function() { $root.currentPageIndex($data) }\"></a></div>\
                                <!-- /ko -->\
                            <!-- /ko -->\
                            <div class=\"small oval default btn\"><a href=\"#\" data-bind=\"click:gotoNextPage\">>></a></div>\
                        </div>\
                    </div>\
                <!-- /ko -->");

    // The "agoGrid" binding
    ko.bindingHandlers.agoGrid = {
        init: function() {
            return { 'controlsDescendantBindings': true };
        },
        // This method is called to initialize the node, and will also be called again if you change what the grid is bound to
        update: function(element, valueAccessor, allBindings, viewModel, bindingContext) {
            var vm = valueAccessor();

            // Empty the element
            while(element.firstChild)
                ko.removeNode(element.firstChild);

            // Allow the default templates to be overridden
            var gridTemplateName   = allBindings.get("agoGridTemplate") || "ko_agoGrid_grid",
                headerTemplateName = allBindings.get("agoGridHeaderTemplate") || "ko_agoGrid_header";
                footerTemplateName = allBindings.get("agoGridFooterTemplate") || "ko_agoGrid_footer";

            // Render the header
            var headerContainer = element.appendChild(document.createElement("DIV"));
            ko.renderTemplate(headerTemplateName, vm, { templateEngine: templateEngine }, headerContainer, "replaceNode");

            // Render the main grid
            var gridContainer = element.appendChild(document.createElement("DIV"));
            ko.renderTemplate(gridTemplateName, vm, { templateEngine: templateEngine }, gridContainer, "replaceNode");

            // Render the footer
            var footerContainer = element.appendChild(document.createElement("DIV"));
            ko.renderTemplate(footerTemplateName, vm, { templateEngine: templateEngine }, footerContainer, "replaceNode");
        }
    };
})();

