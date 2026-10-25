#include "engine/XmlReader.h"

namespace {

std::string decodeEntities(const std::string& s) {
    static const struct {
        const char* entity;
        char c;
    } kEntities[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
    std::string out;
    for (size_t i = 0; i < s.size();) {
        bool matched = false;
        if (s[i] == '&') {
            for (const auto& e : kEntities) {
                size_t n = std::char_traits<char>::length(e.entity);
                if (s.compare(i, n, e.entity) == 0) {
                    out += e.c;
                    i += n;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) out += s[i++];
    }
    return out;
}

std::string tagName(const std::string& xml, size_t from, size_t to) {
    size_t end = from;
    while (end < to && xml[end] != ' ' && xml[end] != '\t' && xml[end] != '\n' && xml[end] != '\r' &&
           xml[end] != '/')
        ++end;
    return xml.substr(from, end - from);
}

}  // namespace

bool parseXml(const std::string& xml, const XmlHandlers& h) {
    size_t i = 0;
    while (i < xml.size()) {
        size_t lt = xml.find('<', i);
        if (lt == std::string::npos) lt = xml.size();
        if (lt > i && h.characters) h.characters(decodeEntities(xml.substr(i, lt - i)));
        if (lt == xml.size()) break;

        if (xml.compare(lt, 4, "<!--") == 0) {
            size_t end = xml.find("-->", lt + 4);
            if (end == std::string::npos) return false;
            i = end + 3;
            continue;
        }
        size_t gt = xml.find('>', lt);
        if (gt == std::string::npos) return false;
        if (xml[lt + 1] == '?' || xml[lt + 1] == '!') {  // prolog / doctype
            i = gt + 1;
            continue;
        }
        if (xml[lt + 1] == '/') {
            if (h.endElement) h.endElement(tagName(xml, lt + 2, gt));
        } else {
            std::string name = tagName(xml, lt + 1, gt);
            if (h.startElement) h.startElement(name);
            if (xml[gt - 1] == '/' && h.endElement) h.endElement(name);  // <empty/>
        }
        i = gt + 1;
    }
    return true;
}
