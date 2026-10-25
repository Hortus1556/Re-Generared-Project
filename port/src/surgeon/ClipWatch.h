// Debug (-d): where the game's wounds are, pass by pass, in debug.log.
//
// The clips watched are the nodes a script attached whose name is one of
// the operation's injury linkages (data/gamedata.json's injuryTypes, the
// scripts attaching each as <linkage><depth>), or the name prefixes in
// AS_WATCH (comma separated) instead. Each is written when it first shows,
// whenever its place, its drawn bounds on the stage, its frame or an inner
// clip's frame changes, and when it goes:
//
//   watch pass 812 mouse (301,188) _root.op.body.underSurfaceExtraction7:
//     frame 3 shown at (120.0,95.5) stage (310.2,240.8) drawn (290,221)-(331,262) a 100; inner holder=2 holder.bullet=5
//
// so a wound drifting away (AS1's bullet hole) shows which of them moved
// it: its own place, its art, or an inner clip's frames. A hidden inner
// clip is marked "(hidden)" (a tool's points done: a cream line rubbed).
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ArtNode;

class ClipWatch {
public:
    ClipWatch();
    void update(ArtNode* stage, long pass, float mouseX, float mouseY);

private:
    bool watched(const std::string& name) const;
    void visit(ArtNode* node, const std::string& path, long pass, float mouseX, float mouseY);
    std::vector<std::string> prefixes;
    std::unordered_map<std::string, std::string> last;  // path -> its last line
    std::unordered_set<std::string> seen;               // paths met this pass
};
