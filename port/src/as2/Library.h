// Library: the injury and tool catalogue.
#pragma once

#include "as2/Connector.h"

namespace as2 {

class Library {
public:
    static Library& sharedLibrary();
    InjuryData* getInjuryByID(int id) const;
    ToolData* getToolByID(int id) const;
    const Array<ToolData>& getToolLibrary() const { return tool_library; }
    // The injury's box {left, top, right, bottom}-like four ints (boundBox).
    const int* getInjuryBoundsForID(int id) const;

private:
    Library();
    Array<InjuryData> injury_library;
    Array<ToolData> tool_library;
};

}  // namespace as2
