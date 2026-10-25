// Minimal event-based XML reader standing in for NSXMLParser and its
// delegate callbacks (didStartElement, foundCharacters, didEndElement).
//
// Covers what the game's dialogue files use: nested elements without
// attributes, character data, comments and an optional <?xml?> prolog.
// Character data between two tags is delivered in one call, whitespace
// included, as NSXMLParser does. The predefined entities (&amp; etc.) are
// decoded.
#pragma once

#include <functional>
#include <string>

struct XmlHandlers {
    std::function<void(const std::string& name)> startElement;
    std::function<void(const std::string& name)> endElement;
    std::function<void(const std::string& text)> characters;
};

// Returns false on malformed input (parsing stops there, as NSXMLParser's
// parse returns NO after reporting the elements read so far).
bool parseXml(const std::string& xml, const XmlHandlers& handlers);
