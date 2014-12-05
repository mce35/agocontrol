/*
 * DOMParser HTML extension
 * 2012-09-04
 * 
 * By Eli Grey, http://eligrey.com
 * Public domain.
 * NO WARRANTY EXPRESSED OR IMPLIED. USE AT YOUR OWN RISK.
 */

/*! @source https://gist.github.com/1129031 */
/*global document, DOMParser*/

(function(DOMParser) {
    "use strict";

    var DOMParser_proto = DOMParser.prototype, real_parseFromString = DOMParser_proto.parseFromString;

    // Firefox/Opera/IE throw errors on unsupported types
    try {
	// WebKit returns null on unsupported types
	if ((new DOMParser).parseFromString("", "text/html")) {
	    // text/html parsing is natively supported
	    return;
	}
    } catch (ex) {
    }

    DOMParser_proto.parseFromString = function(markup, type) {
	if (/^\s*text\/html\s*(?:;|$)/i.test(type)) {
	    var doc = document.implementation.createHTMLDocument("");
	    if (markup.toLowerCase().indexOf('<!doctype') > -1) {
		doc.documentElement.innerHTML = markup;
	    } else {
		doc.body.innerHTML = markup;
	    }
	    return doc;
	} else {
	    return real_parseFromString.apply(this, arguments);
	}
    };
}(DOMParser));

function xml2string(node) {
    if (typeof (XMLSerializer) !== 'undefined') {
	var serializer = new XMLSerializer();
	return serializer.serializeToString(node);
    } else if (node.xml) {
	return node.xml;
    }
}

function getBrowserLang() {
    var lang = navigator.language || navigator.userLanguage;
    return lang.split("_")[0].split("-")[0];
}

function prepareTemplate(doc) {
    var targetLang = getBrowserLang();

    var supportedLanguages = {};
    var tags = doc.getElementsByTagName("*");

    /* First find out which languages we support */
    for ( var i = 0; i < tags.length; i++) {
	var tag = tags[i];
	if (tag.getAttribute("data-translateable") == null) {
	    continue;
	}
	var lang = tag.getAttribute("xml:lang");
	if (lang != null) {
	    supportedLanguages[lang] = true;
	}
    }

    targetLang = supportedLanguages[targetLang] ? targetLang : null;

    /* Remove tags which have a different language */
    var killList = [];
    for ( var i = 0; i < tags.length; i++) {
	var tag = tags[i];
	if (tag.getAttribute("data-translateable") == null) {
	    continue;
	}
	var lang = tag.getAttribute("xml:lang");
	if (lang != targetLang) {
	    killList.push(tag);
	}
    }

    for ( var i = 0; i < killList.length; i++) {
	killList[i].parentNode.removeChild(killList[i]);
    }

    return xml2string(doc);
}

