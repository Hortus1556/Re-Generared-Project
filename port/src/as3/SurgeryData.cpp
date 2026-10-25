#include "as3/SurgeryData.h"

#include "as3/Content.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/RestoredContent.h"

namespace mt {

using cocos2d::ccp;

const char* const kToolNames[kToolCount] = {"Lighter", "Cream",     "Stapler", "Cutter", "Tazer",  "Tongs",
                                            "Chainsaw", "Injection", "Vacuum",  "Clamp",  "Scanner"};

const char* const kInjuryBaseNames[kInjuryBaseCount] = {
    "AngledExtraction", "Artery", "Bat", "BlackFrog", "Boil", "Burn", "CementDrop", "FireMaggot", "Flea",
    "Incision", "LargeCut", "Leech", "MiniBat", "MiniFrog", "PassiveParticle", "PoisonArea", "RobotScorpion",
    "SmallCut", "Snake", "SoakableParticle", "Transplant", "UnderSurfaceExtraction", "BrokenBone", "BrokenVein",
    "Fire", "PoisonFrog", "SurfaceExtraction", "IceAnt", "LargeCutRobot", "SmallCutRobot", "IncisionRobot",
    "Rust", "SnakeCut", "SuperPoisonArea", "SuperPoisonScab", "FrostBite", "SpikeBase", "MiniWorm", "GiantWorm",
    "Spider", "Ghost", "Locust", "Jellyfish", "Pygmy"};

namespace {

const char* const kClassNames[kClassNameCount] = {
    "AngledExtractionSnake", "BatFang", "LeechTeeth", "MutabearExtractionA", "Spike", "Splinter",
    "Toothbrush_Yellow", "Toothbrush_Orange", "Toothbrush_Green", "Toothbrush_Pink", "Razor", "Razorblade",
    "RoborpionShard1", "RoborpionShard2", "RoborpionShard3", "RoborpionShard4", "Artery", "Bat", "BlackFrog",
    "Boil", "Burn", "CementDrop", "FireMaggot", "Flea", "Incision", "MutabearIncisionA", "LargeCut", "BatBite",
    "Leech", "MiniBat", "MiniFrog", "PassiveParticle", "PoisonArea", "RobotScorpion", "SmallCut", "Snake",
    "SnakeCut", "SoakableParticle", "HeartTransplant", "KidneyTransplant", "LiverTransplant", "LungTransplant",
    "MutaHeartTransplant", "TankTransplant", "cementlung", "cementstomach", "UnderSurfaceSplinter",
    "UnderSurfaceRazorblade", "BrokenLeg", "BrokenLegVein", "LegRightTransplant", "Stake",
    "PoisonStomachTransplant", "Piranha", "PiranhaCut", "kneeRTransplant", "kneeLTransplant", "ribs1", "ribs2",
    "ribs3", "LegLeftTransplant", "FrogPoisonArea", "cementlungL", "Fire", "PoisonFrog", "PoisonDart",
    "FrogPoisonStomachTransplant", "SpineUpperPiece1Transplant", "SpineUpperPiece2Transplant",
    "SpineLowerPiece1Transplant", "SpineLowerPiece2Transplant", "GrindarellaArmLTransplant",
    "GrindarellaLegLTransplant", "GrindarellaArmRTransplant", "GrindarellaLegRTransplant", "IceAnt",
    "Fuse1Transplant", "Fuse2Transplant", "Fuse3Transplant", "EyeRobotTransplant", "MicrochipTransplant",
    "LargeCutRobot", "SmallCutRobot", "IncisionRobot", "Rust", "SuperPoisonArea", "SuperPoisonScab", "FrostBite",
    "RobotBrokenLeg", "RobotBrokenLegVein", "BumpsLegLTransplant", "kneeR2Transplant", "SpikeClumpTransplant",
    "SpikeBase", "SpikeRoot", "GooTub", "MonitorShard3", "MonitorShard2", "MonitorShard1", "LungTransplantL",
    "MiniWorm", "GiantWorm", "WormStuck", "GiantWormWrap", "MiniwormCut", "WormCut", "WormExtract1",
    "WormExtract2", "GiantWormLoopIncision", "GiantWormFront", "GiantWormIncision", "GiantWormChestWrap",
    "UnderSurfaceSpikeRoot", "UnderSurfaceSpike",
    // the Hospital's (Android 1.43)
    "DarkPoisonArea", "Spider", "Ghost", "Coffeebean1", "Coffeebean2", "Coffeebean3", "emohamster", "chastityBelt",
    "chastityLock", "Coffeecup", "Coffeemug", "pentagram", "pentagram2", "BlackHeartTransplant", "bugArm",
    "gossamerHead", "hand", "eggpiece1", "eggpiece2", "brokenheart1", "brokenheart2", "sadeyes", "frown",
    "glasses", "sadface", "handR", "bugArmR", "UnderSurfaceCoffeebean1", "UnderSurfaceCoffeebean2",
    "UnderSurfaceCoffeebean3", "UnderSurfaceHamster", "UnderSurfacesadface", "IncisionLock", "coffeeartery",
    // Le Flairground Fantastique's (iOS 1.45; its own table puts the first nine among the old names)
    "JellyfishSting", "Barnacle", "Brimstone", "Locust", "Jellyfish", "AshParticle", "Pygmy", "PygmyHalf", "Match",
    "PG_Octopus1", "PG_Octopus2", "PG_Octopus3", "PG_PalmTree", "BugNet", "BugSpray", "PG_Pygmy1", "PG_Pygmy2",
    "PG_Pygmy3", "PG_Shark", "PG_Puffin", "IncisionWhale", "PG_Match", "UnderSurfacePG_Match", "octopusIdol1",
    "octopusIdol2", "octopusIdol3", "armBone", "whaletooth", "whaleribs1", "UnderSurfaceBugNet",
    "UnderSurfaceBugSpray", "UnderSurfacePG_PalmTree"};

std::string asString(const json::Value& v) { return v.type == json::Value::kString ? v.string : ""; }
int asInt(const json::Value& v) {
    if (v.type == json::Value::kNumber) return static_cast<int>(v.number);
    if (v.type == json::Value::kBool) return v.boolean ? 1 : 0;
    return 0;
}
float asFloat(const json::Value& v) { return static_cast<float>(v.num()); }
bool asBool(const json::Value& v) {
    if (v.type == json::Value::kBool) return v.boolean;
    if (v.type == json::Value::kNumber) return v.number != 0;
    return false;
}

int lookup(const char* const* names, int count, const std::string& name, int fallback) {
    for (int i = 0; i < count; ++i)
        if (name == names[i]) return i;
    return fallback;
}

// The hit area groups (0x90058): each element of a group is a hit area for
// the group's state.
void readHitAreas(const json::Value& root, InjuryDef& def) {
    int radius = hasBigHitAreas(def) ? 35 : 25;
    for (const json::Value& group : root["hit_area_groups"].items) {
        int state = stateByName(asString(group["name"]));
        for (const json::Value& e : group["elements"].items) {
            HitArea a;
            a.state = state;
            a.name = asString(e["name"]);
            a.forClamp = e.has("for_clamp") && asBool(e["for_clamp"]);
            for (const json::Value& p : e["points"].items) {
                if (a.points.size() >= 50) break;
                HitPoint h;
                h.x = asInt(p["x"]);
                h.y = asInt(p["y"]);
                h.angle = asInt(p["angleInDegrees"]);
                h.radius = a.forClamp ? 85 : radius;
                a.points.push_back(h);
            }
            def.hitAreas.push_back(a);
        }
    }
}

}  // namespace

const char* const kStateNames[kStateCount] = {
    "Open", "OpenWithClamp", "Closed", "Burnt", "Fixed", "Free", "Replacing", "Replaced", "Extracting",
    "BatHanging", "BatFlying", "BatDied", "MiniBatFlying", "Growing", "PoisonAreaIdle", "PoisonAreaEmitting",
    "SoakableParticleIdle", "PassiveParticleIdle", "Sucking", "Hidden", "FleaJumping", "FleaDead",
    "RobotScorpionEmerging", "RobotScorpionDashing", "RobotScorpionBurying", "RobotScorpionTurning",
    "RobotScorpionSubmerged", "RobotScorpionCharging", "RobotScorpionFiring", "RobotScorpionTazered",
    "RobotScorpionClamped", "RobotScorpionExploding", "RobotScorpionDead", "Broken", "stage1", "stage2",
    "stage3", "Clamped", "SnakeInHole", "SnakeOut", "FireEmitting", "PoisonFrogIdle", "PoisonFrogDead",
    "PoisonFrogHurt", "PoisonFrogShocked", "PoisonFrogWalking", "PoisonFrogAngry", "PoisonFrogFiring",
    "PoisonFrogLooking", "FireMaggotCalmed", "FireMaggotAngry", "FireMaggotDead", "IceAntWalking", "IceAntDead",
    "IceAntBite", "IceAntTazer", "BlackFrogNormal", "BlackFrogAngry", "BlackFrogDead", "MiniFrogEggClosed",
    "MiniFrogEggClamped", "MiniFrogNormal", "MiniFrogAngry", "MiniFrogDead", "Shut", "MiniWormAlive",
    "MiniWormBite", "MiniWormMove", "MiniWormLatched", "MiniWormDangling", "MiniWormShocked", "GiantWormAlive",
    "GiantWormDead", "GiantWormLookingAround", "GiantWormMoving", "GiantWormSpittingAtScreen",
    "GiantWormSpittingAtPatient", "GiantWormGoingUnder", "GiantWormUnderSkin", "GiantWormReact",
    "GiantWormWrapChest", "IncisionShut",
    // the Hospital's (Android 1.43)
    "SpiderWalking", "SpiderDead", "SpiderBite", "GhostWalking", "GhostInvisible", "GhostStruggle", "GhostDying",
    // Le Flairground Fantastique's (iOS 1.45)
    "JellyfishSwimming", "JellyfishFiring", "JellyfishDied", "LocustJumping", "LocustDead", "PygmyAlive",
    "PygmyLeftHalf", "PygmyRightHalf", "PygmyBurnt", "PygmyInjured"};

int toolByName(const std::string& name) { return lookup(kToolNames, kToolCount, name, kNoTool); }
int injuryClassIndex(const std::string& name) { return lookup(kClassNames, kClassNameCount, name, 0); }
int stateByName(const std::string& name) { return lookup(kStateNames, kStateCount, name, 0); }

int InjuryDef::stageOf(int state) const {
    for (size_t i = 0; i < stages.size(); ++i)
        if (stages[i].state == state) return static_cast<int>(i) + 1;
    return 0;
}

bool isSmallScreen() {
    // (1.45 0x9aaf4: under 720 wide or 480 high; 1.02's 0x75e5c: at most
    // 480 wide or 320 high. The port's 568x320 is small by both)
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    if (r->width / r->canvasScale < 720.0f) return true;
    return r->height / r->canvasScale < 480.0f;
}

bool hasBigHitAreas(const InjuryDef& def) {
    // AngledExtraction, Burn, LargeCut, SmallCut, BrokenVein, SurfaceExtraction
    return isSmallScreen() && def.base < 27 && ((0x4220421u >> def.base) & 1);
}

InjuryDef loadInjuryDef(const std::string& className) {
    InjuryDef def;
    json::Value root;
    // (the port's: a restored cut injury the package has no file for, RestoredContent.h)
    if (!restored::injuryJson(className, root)) readJson("Data/Injuries/" + className + ".json", root);
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;

    std::string baseName = asString(root["base_class_name"]);
    std::string name = asString(root["class_name"]);
    if (name.empty()) name = baseName;
    def.name = name;
    def.baseName = baseName;
    def.base = lookup(kInjuryBaseNames, kInjuryBaseCount, baseName, 0);
    def.classIndex = injuryClassIndex(name);
    // 1.25 times bigger on a small screen: the original asked 0x75eb0 before
    // it had read the base, still 0 (AngledExtraction, one of the bases with
    // big hit areas), so every injury gets it
    float widen = isSmallScreen() ? 1.25f : 1.0f;
    def.removeOnComplete = asBool(root["remove_on_complete"]);
    const json::Value& size = root["size"];
    def.size = CCSizeMake(asInt(size["width"]) * s * widen, asInt(size["height"]) * s * widen);
    def.aux1 = asInt(root["aux_1"]);
    def.isLimb = asBool(root["is_limb"]);
    def.depth = asInt(root["depth"]);
    def.passiveFxRef = asString(root["passive_fx_ref"]);
    def.passiveSpriteRef = asString(root["passive_sprite_ref"]);
    def.soakableFxRef = asString(root["soakable_fx_ref"]);
    def.soakableSpriteRef = asString(root["soakable_sprite_ref"]);
    def.bubbleFxRef = asString(root["bubble_fx_ref"]);
    def.embedded = root.has("embedded") ? asBool(root["embedded"]) : true;
    if (root.has("sprites"))
        for (const json::Value& v : root["sprites"].items) {
            InjuryPieceDef p;
            p.sprite = asString(v);
            def.pieces.push_back(p);
        }

    for (const json::Value& st : root["stages"].items) {
        InjuryStageDef d;
        d.needsGraphic = asBool(st["needs_graphic"]);
        for (const json::Value& f : st["animation_filenames"].items)
            if (d.animationFilenames.size() < 2) d.animationFilenames.push_back(asString(f));
        d.useSizeRect = asBool(st["use_size_rect"]);
        d.showHitAreas = asBool(st["show_hit_areas"]);
        d.canBeGrabbed = asBool(st["can_be_grabbed"]);
        d.state = stateByName(asString(st["state"]));
        d.stateName = asString(st["state"]);
        d.damageTimer = asFloat(st["damage_timer"]);
        d.damage = asFloat(st["damage"]);
        d.particleTimer = asFloat(st["particle_timer"]);
        d.particle = 0;  // only "Blood" (0) exists; anything else also read as 0
        d.eventTimer = asFloat(st["event_timer"]);
        d.createAtStart = asString(st["injury_to_create_at_start"]);
        d.createAtStartOffset = ccp(static_cast<float>(asInt(st["injury_to_create_at_start_offset_x"])),
                                    static_cast<float>(asInt(st["injury_to_create_at_start_offset_y"])));
        d.createAtEnd = asString(st["injury_to_create_at_end"]);
        d.createAtEndOffset = ccp(static_cast<float>(asInt(st["injury_to_create_at_end_offset_x"])),
                                  static_cast<float>(asInt(st["injury_to_create_at_end_offset_y"])));
        d.createOnExtraction = asString(st["injury_to_create_on_extraction"]);
        for (const json::Value& t : st["tools"].items)
            if (d.tools.size() < 10) d.tools.push_back(toolByName(asString(t)) == kNoTool ? 0 : toolByName(asString(t)));
        def.stages.push_back(d);
    }
    readHitAreas(root, def);

    // the pieces from a data file of the art (`injury_data_file`)
    std::string dataFile = asString(root["injury_data_file"]);
    if (!dataFile.empty()) {
        json::Value data;
        readJson("common/" + dataFile + ".json", data);  // (0x97d38: "common/<name>.json")
        readHitAreas(data, def);
        const json::Value& dsize = data["size"];
        def.size = CCSizeMake(asInt(dsize["width"]) * s, asInt(dsize["height"]) * s);
        for (const json::Value& sp : data["sprites"].items) {
            InjuryPieceDef p;
            p.sprite = asString(sp["sprite"]);
            const json::Value& r = sp["activeRect"];
            p.activeRect = CCRectMake(asFloat(r["x"]) * s, asFloat(r["y"]) * s, asInt(r["width"]) * s,
                                      asInt(r["height"]) * s);
            p.isReplacement = asBool(sp["isReplacement"]);
            p.zOrder = asInt(sp["zOrder"]);
            p.interactable = sp.has("interactable") ? asBool(sp["interactable"]) : true;
            if (sp["tint"].items.size() == 3)
                p.tint = cocos2d::ccc3(static_cast<GLubyte>(asInt(sp["tint"].items[0])), static_cast<GLubyte>(asInt(sp["tint"].items[1])),
                              static_cast<GLubyte>(asInt(sp["tint"].items[2])));
            for (const json::Value& dep : sp["dependencies"].items) {
                std::string n = asString(dep);
                for (const HitArea& a : def.hitAreas)
                    if (a.name == n) {
                        p.dependencies.push_back(a);  // (0x81a64: the last of that name)
                    }
            }
            if (p.isReplacement) p.kind = 6;
            else p.notReplacement = true;
            def.pieces.push_back(p);
        }
    } else if (def.base == kAngledExtraction && def.pieces.empty()) {
        // (the original made one empty piece for base 0 with none)
        def.pieces.push_back(InjuryPieceDef());
    }
    return def;
}

std::string surgeryFile(const std::string& surgeryId, int mode, const std::string& customFile) {
    if (!customFile.empty()) return customFile;
    std::string name = surgeryId;
    if (mode == 1) name += "_challenge1";
    else if (mode == 2) name += "_challenge2";
    else if (mode != 0) logError("Unrecognized game mode");
    return "Surgeries/" + name + ".json";  // in the downloaded content
}

SurgeryDef loadSurgeryDef(const json::Value& root) {
    SurgeryDef d;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (640.0f - r->width / s) * 0.5f, my = (384.0f - r->height / s) * 0.5f;
    for (const json::Value& v : root["sprite_sheets"].items) d.sheets.push_back(asString(v));
    d.isChallenge = asBool(root["is_challenge"]);
    d.isSuddenDeath = asBool(root["is_sudden_death"]);
    d.tagAllowed = root.has("tag_allowed") ? asBool(root["tag_allowed"]) : true;
    d.injuryDamageMultiplier = root.has("injury_damage_multiplier") ? asFloat(root["injury_damage_multiplier"]) : -1;
    for (const json::Value& v : root["sounds"].items) d.sounds.push_back(asString(v));
    d.scream = asString(root["scream"]);
    for (int i = 0; i < 3; ++i) {
        d.minScore[i] = asInt(root["min_score_to_" + std::to_string(i + 1) + "_star"]);
        d.minTime[i] = asInt(root["min_time_to_" + std::to_string(i + 1) + "_star"]);
    }
    d.tutorial = asString(root["tutorial_filename"]);
    d.hideExtraction = root.has("hide_extraction") && asBool(root["hide_extraction"]);
    for (const json::Value& t : root["tools"].items) {
        int tool = toolByName(asString(t));
        d.tools.push_back(tool == kNoTool ? 0 : tool);
    }
    for (const json::Value& st : root["stages"].items) {
        SurgeryStageDef g;
        g.name = asInt(st["name"]);
        g.bg = asString(st["bg"]);
        g.bgFlipX = asBool(st["bg_flip_x"]);
        g.bgFlipY = asBool(st["bg_flip_y"]);
        g.effect = st.has("effect") ? asString(st["effect"]) : "";
        g.injuryDamageMultiplier = st.has("injury_damage_multiplier") ? asFloat(st["injury_damage_multiplier"]) : -1;
        for (const json::Value& b : st["additional_bgs"].items) {
            AdditionalBg a;
            a.filename = asString(b["filename"]);
            a.x = asInt(b["x"]);
            a.y = asInt(b["y"]);
            a.rotation = asInt(b["rotation"]);
            a.flipX = asBool(b["flip_x"]);
            a.flipY = asBool(b["flip_y"]);
            g.additionalBgs.push_back(a);
        }
        g.bgm = asString(st["bgm"]);
        g.bodyLevel = asInt(st["body_level"]);
        g.toSurgeryStage = asInt(st["to_surgery_stage"]);
        g.health = asInt(st["health"]);
        g.time = asInt(st["time"]);
        for (const json::Value& inj : st["injuries"].items) {
            StageInjury i;
            i.className = asString(inj["class_name"]);
            // canvas points to the screen's
            i.position = ccp((asInt(inj["x"]) - mx) * s, (asInt(inj["y"]) - my) * s);
            i.rotation = asInt(inj["rotation"]);
            i.allowNonEssentialCreation =
                inj.has("allow_non_ess_injury_creation") ? asBool(inj["allow_non_ess_injury_creation"]) : true;
            for (const json::Value& v : inj["visual_dependencies"].items) i.visualDependencies.push_back(asString(v));
            for (const json::Value& v : inj["interaction_dependencies"].items)
                i.interactionDependencies.push_back(asString(v));
            i.name = asString(inj["name"]);
            i.toSurgeryStage = asInt(inj["to_surgery_stage"]);
            if (inj.has("stage_end_to_swap")) i.stageEndToSwap = asInt(inj["stage_end_to_swap"]);
            g.injuries.push_back(i);
        }
        d.stages.push_back(g);
    }
    return d;
}

}  // namespace mt
