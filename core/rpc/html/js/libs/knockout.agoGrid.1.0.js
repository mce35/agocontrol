/**
 * agoGrid is pure knockout bootstraped grid
 * agoGrid is based on simpleGrid from knockoutjs website (http://knockoutjs.com/examples/grid.html)
 *
 * Benefits over jquery-datatables:
 *  - lighter (nearly the same size than bindings file)
 *  - supported (jquery-datatables is maintained but not knockout.bindings.dataTables.js)
 *  - no language pack needed
 *  - fits to agocontrol needs
 *  - pure knockout
 *  - bootstraped
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
 *              - headerText : displayed column name [MANDATORY]. If headerText='' the column is hidden (useful to specify search strings)
 *              - rowText: name of real column name in data item [MANDATORY]. If rowText='' the column is not used for searches (useful for toolbar)
 *             Sample:
 *              columns: [{headerText:'President', rowText:'name'}, {headerText:'Took office', rowText:'took'}, {headerText:'Left office', rowText:'left'}]
 *              data: [{name:'George Washington', took:'April 30, 1789', left:'March 4, 1797'}, {name:'John Adams', took:'March 4, 1797', left:'March 4, 1801'}] 
 *  - rowCallback: callback when row is clicked. Useful for jquery-jeditable.
 *  - rowTemplate: specify custom knockout template name (http://knockoutjs.com/documentation/template-binding.html).
 *  - pageSize: change page size at startup (can be 10, 25, 50 or 100) [DEFAULT=10].
 *  - displayRowCount: display row count (bottom left) [DEFAULT=true]
 *  - displaySearch: display search box (top right) [DEFAULT=true]
 *  - displayPagination: display pagination (top left and bottom right) [DEFAULT=true]
 *  - gridId: set grid table id [DEFAULT=agoGrid]
 *  - boxStyle: set box style. See adminLTE for available style [DEFAULT='box-default']
 *  - boxTitle: set box title. [DEFAULT='']
 *  - tableStyle: set table style. See bootstrap for available table style [DEFAULT='table-hover table-bordered table-stripped']
 *  - defaultSort: set default sort [DEFAULT=first grid key]
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
        viewModel: function(configuration) {
            this.allData = configuration.data;
            this.data = configuration.data;
            this.displayRowCount = configuration.displayRowCount===undefined ? true : configuration.displayRowCount;
            this.displaySearch = configuration.displaySearch===undefined ? true : configuration.displaySearch;
            this.displayPagination = configuration.displayPagination===undefined ? true : configuration.displayPagination;
            this.rowCallback = configuration.rowCallback;
            this.currentPageIndex = ko.observable(0);
            this.pageSize = ko.observable(configuration.pageSize || 10);
            this.pageSizes = ko.observableArray([10, 25, 50, 100]);
            this.hasPreviousPage = ko.observable(false);
            this.hasNextPage = ko.observable(false);
            this.search = ko.observable('');
            this.filters = ko.observableArray([]);
            this.gridId = configuration.gridId || 'agoGrid';
            // If you don't specify columns configuration, we'll use scaffolding
            this.columns = configuration.columns || getColumnsForScaffolding(ko.unwrap(this.data));
            this.sortKey = ko.observable(configuration.defaultSort ? configuration.defaultSort : this.columns.length>0 ? this.columns[0].rowText : '');
            this.sortAsc = ko.observable(true);
            this.rowTemplate = configuration.rowTemplate || "ko_agoGrid_row";
            this.bodyTemplate = configuration.rowTemplate ? "ko_agoGrid_customBody" : "ko_agoGrid_body";
            this.boxStyle = configuration.boxStyle ? configuration.boxStyle : "box-default";
            this.boxTitle = configuration.boxTitle ? $.trim(configuration.boxTitle) : "";
            this.tableStyle = ko.observable(configuration.tableStyle ? configuration.tableStyle : "table-hover table-bordered table-stripped");

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
                        var aKey = ko.utils.unwrapObservable(a[key]);
                        var bKey = ko.utils.unwrapObservable(b[key]);
                        if( typeof a[key]==='string' || typeof a[key]==='object' )
                        {
                            aKey = (''+a[key]).toLowerCase();
                            bKey = (''+b[key]).toLowerCase();
                        }
                        if( asc )
                        {
                            if( aKey < bKey )
                                return -1;
                            else if( aKey > bKey )
                                return 1;
                            return 0;
                        }
                        else
                        {
                            if( aKey > bKey )
                                return -1;
                            else if( aKey < bKey )
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
                if( this.displayPagination )
                {
                    var startIndex = this.pageSize() * this.currentPageIndex();
                    return ko.unwrap(this.data).slice(startIndex, startIndex + this.pageSize());
                }
                else
                {
                    return ko.unwrap(this.data);
                }
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
    
            this.sortBy = function(agoGrid, key) {
                if( key && key.length>0 )
                {
                    if( key!=agoGrid.sortKey() )
                    {
                        agoGrid.sortAsc(true);
                        agoGrid.sortKey(key);
                    }
                    else
                    {
                        agoGrid.sortAsc(!agoGrid.sortAsc());
                    }
                }
            };

            this.onCellClick = function(agoGrid, item, evt) {
                if( agoGrid.rowCallback )
                {
                    //callback with parameters: clicked item, clicked cell (td), clicked row (tr)
                    agoGrid.rowCallback(item, evt.target, evt.target.parentElement);
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
                <!-- ko if: displayPagination||displaySearch -->\
                <div class=\"row\" style=\"padding: 10px 15px 10px 15px;\">\
                    <div class=\"col-md-5 hidden-xs\">\
                        <!-- ko if:displayPagination -->\
                        <select class=\"form-control input-sm\" data-bind=\"options:pageSizes, value:pageSize\" style=\"max-width:100px;\"></select>\
                        <!-- /ko -->\
                    </div>\
                    <div class=\"col-xs-9 col-md-offset-2 col-md-5\" style=\"text-align:right;\">\
                        <!-- ko if:displaySearch -->\
                        <form class=\"form-inline\">\
                            <span class=\"en-search hidden-xs\"></span>\
                            <div class=\"input-group input-group-sm\">\
                                <input type=\"text\" class=\"form-control\" data-bind=\"textInput:search\">\
                                <span class=\"input-group-btn\">\
                                    <button class=\"btn btn-primary\" type=\"button\" data-bind=\"click:function(){clearSearch();}\"><span class=\"en-cancel\"/></button>\
                                </span>\
                            </div>\
                        </form>\
                        <!-- /ko -->\
                    </div>\
                </div>\
                <!-- /ko -->");

    //default grid template
    templateEngine.addTemplate("ko_agoGrid_grid", "\
                    <div class=\"table-responsive\"> \
                    <table class=\"table table-hover table-bordered table-stripped\" style=\"margin-top:0px; margin-bottom:0px;\" data-bind=\"attr: {id:gridId}\"> \
                        <thead>\
                            <tr data-bind=\"foreach: columns\">\
                               <!-- ko ifnot: headerText=='' -->\
                               <th>\
                                  <!-- ko ifnot: rowText=='' -->\
                                      <span style=\"cursor:pointer; text-overflow:ellipsis;\" data-bind=\"text:headerText, click:sortBy.bind($data, $context, rowText)\"></span>\
                                      <!-- ko if:rowText==sortKey() -->\
                                          <!-- ko if:sortAsc() -->\
                                              <span class=\"en-down-dir\"></span>\
                                          <!-- /ko -->\
                                          <!-- ko ifnot:sortAsc() -->\
                                              <span class=\"en-up-dir\"></span>\
                                          <!-- /ko -->\
                                      <!-- /ko -->\
                                  <!-- /ko -->\
                                  <!-- ko if: rowText=='' -->\
                                      <span style=\"text-overflow:ellipsis;\" data-bind=\"text:headerText\"></span>\
                                  <!-- /ko -->\
                               </th>\
                               <!-- /ko -->\
                            </tr>\
                        </thead>\
                        <tbody data-bind=\"template : { name:bodyTemplate, foreach:itemsOnCurrentPage, as:'myrow' }\">\
                        </tbody>\
                    </table> \
                    </div>");

    //default body template
    templateEngine.addTemplate("ko_agoGrid_body", "\
                            <tr data-bind=\"foreach:columns, click:onCellClick.bind($data, $context)\">\
                               <td data-bind=\"text: typeof rowText == 'function' ? rowText($parent) : $parent[rowText]\"></td>\
                            </tr>");

    //template used when user provide custom row template
    templateEngine.addTemplate("ko_agoGrid_customBody", "\
                            <tr data-bind=\"template: { name:rowTemplate }, click:onCellClick.bind($data, $context)\">\
                            </tr>");

    //default footer template
    templateEngine.addTemplate("ko_agoGrid_footer", "\
                    <!-- ko if: displayPagination||displayRowCount -->\
                    <div class=\"row\" style=\"padding: 10px 15px 10px 15px;\">\
                        <div class=\"col-md-4 hidden-xs\">\
                            <!-- ko if:displayRowCount -->\
                            <span data-bind=\"text:range\" style=\"font-weight:bold;\"></span>\
                            <!-- /ko -->\
                        </div>\
                        <div class=\"col-xs-12 col-md-8\" style=\"text-align:right;\">\
                            <!-- ko if:displayPagination -->\
                            <ul class=\"pagination pagination-sm\" style=\"margin: 0px !important;\">\
                                <li>\
                                    <a href=\"#\" data-bind=\"click:function() { gotoPreviousPage(); }\" aria-label=\"Previous\">\
                                        <span aria-hidden=\"true\">&laquo;</span>\
                                    </a>\
                                </li>\
                                <!-- ko foreach: ko.utils.range(0, maxPageIndex) -->\
                                    <!-- ko if:$data==currentPageIndex() -->\
                                    <li class=\"active\">\
                                        <a href=\"#\" data-bind=\"text:$data+1, click:function() { currentPageIndex($data); }\"></a>\
                                    </li>\
                                    <!-- /ko -->\
                                    <!-- ko ifnot:$data==currentPageIndex() -->\
                                    <li>\
                                        <a href=\"#\" data-bind=\"text:$data+1, click:function() { currentPageIndex($data); }\"></a>\
                                    </li>\
                                    <!-- /ko -->\
                                <!-- /ko -->\
                                <li>\
                                    <a href=\"#\" data-bind=\"click:function() { gotoNextPage(); }\" aria-label=\"Next\">\
                                        <span aria-hidden=\"true\">&raquo;</span>\
                                    </a>\
                                </li>\
                            </ul>\
                            <!-- /ko -->\
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
            // Empty the element
            while(element.firstChild)
                ko.removeNode(element.firstChild);

            var childBindingContext = bindingContext.createChildContext(
                    bindingContext.$rawData,
                    '$template',
                    function(context) {
                        ko.utils.extend(context, valueAccessor());
                    }
            );

            //get options
            var options = valueAccessor();

            // Allow the default templates to be overridden
            var gridTemplateName   = allBindings.get("agoGridTemplate") || "ko_agoGrid_grid",
                headerTemplateName = allBindings.get("agoGridHeaderTemplate") || "ko_agoGrid_header";
                footerTemplateName = allBindings.get("agoGridFooterTemplate") || "ko_agoGrid_footer";

            var container = element.appendChild(document.createElement("DIV"));
            $(container).addClass("box");
            $(container).addClass(options.boxStyle);

            // Render box header
            if( options.boxTitle.length>0 )
            {
                var boxHeader = container.appendChild(document.createElement("DIV"));
                $(boxHeader).addClass("box-header with-border");
                var boxTitle = boxHeader.appendChild(document.createElement("H3"));
                $(boxTitle).addClass("box-title");
                $(boxTitle).text(options.boxTitle);
            }

            // Render box body
            var boxBody = container.appendChild(document.createElement("DIV"));
            $(boxBody).addClass("box-body no-padding");

            // Render box body header
            var headerContainer = boxBody.appendChild(document.createElement("DIV"));
            ko.renderTemplate(headerTemplateName, childBindingContext, { templateEngine: templateEngine }, headerContainer, "replaceNode");
            
            // Render box body grid
            var gridContainer = boxBody.appendChild(document.createElement("DIV"));
            ko.renderTemplate(gridTemplateName, childBindingContext, { templateEngine: templateEngine }, gridContainer, "replaceNode");

            // Render box body footer
            var footerContainer = boxBody.appendChild(document.createElement("DIV"));
            ko.renderTemplate(footerTemplateName, childBindingContext, { templateEngine: templateEngine }, footerContainer, "replaceNode");
        }
    };
})();

