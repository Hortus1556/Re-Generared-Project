#include "as2/Registry.h"

#include "as2/GameController.h"
#include "as2/MenuManager.h"
#include "as2/injuries/Barnacle.h"
#include "as2/injuries/Bat.h"
#include "as2/injuries/Centipede.h"
#include "as2/injuries/Cosmetic.h"
#include "as2/injuries/Crab.h"
#include "as2/injuries/Cuts.h"
#include "as2/injuries/Dove.h"
#include "as2/injuries/Explosive.h"
#include "as2/injuries/Extraction.h"
#include "as2/injuries/Remains.h"
#include "as2/injuries/Eyelid.h"
#include "as2/injuries/Fire.h"
#include "as2/injuries/Goo.h"
#include "as2/injuries/Graveyard.h"
#include "as2/injuries/FireFly.h"
#include "as2/injuries/Insertion.h"
#include "as2/injuries/Poison.h"
#include "as2/injuries/SimonSays.h"
#include "as2/injuries/Snake.h"
#include "as2/injuries/Splatter.h"
#include "as2/injuries/AmpPlug.h"
#include "as2/injuries/Puslet.h"
#include "as2/injuries/BigTick.h"
#include "as2/injuries/Skull.h"
#include "as2/injuries/Squid.h"
#include "as2/injuries/Transistor.h"
#include "as2/injuries/Tick.h"
#include "as2/injuries/Transplant.h"
#include "as2/injuries/TypeAndTell.h"
#include "as2/menus/CutScene.h"
#include "as2/menus/InGame.h"
#include "as2/menus/LevelSelect.h"
#include "as2/menus/MainMenu.h"
#include "as2/menus/Panel.h"
#include "as2/menus/Results.h"
#include "as2/menus/Screens.h"
#include "as2/tools/Stitch.h"
#include "as2/tools/Battery.h"
#include "as2/tools/Chainsaw.h"
#include "as2/tools/Injection.h"
#include "as2/tools/Scanner.h"
#include "as2/tools/Tongs.h"
#include "as2/tools/Vacuum.h"
#include "as2/tools/Tools.h"

namespace as2 {

namespace {
template <class VC>
void controller(const char* name) {
    MenuManager::registerViewController(name, [](const std::string& nib) -> ui::ViewController* { return new VC(nib); });
}
template <class T>
void injury(const char* name) {
    registerInjuryClass(name, [](InjuryData* d, Injury* i, bool pc, bool f, int) -> InjuryGraphic* {
        return new T(d, i, pc, f);
    });
}
// A class with the five-argument initialiser (initWith...Velocity:).
template <class T>
void injuryWithVelocity(const char* name) {
    registerInjuryClass(name, [](InjuryData* d, Injury* i, bool pc, bool f, int v) -> InjuryGraphic* {
        return new T(d, i, pc, f, v);
    });
}
template <class T>
void tool(const char* name) {
    registerToolClass(name, [](ToolData* d) -> ITool* { return new T(d); });
}
}  // namespace

void registerClasses() {
    ui::registerViewClass("PCGImageView", [] { return new PCGImageView; });
    ui::registerViewClass("EAGLView", [] { return new EAGLView; });
    controller<MainMenu>("MainMenu");
    controller<FileSelect>("FileSelect");
    controller<LevelSelect>("LevelSelect");
    controller<CutScene>("CutScene");
    controller<StartScene>("StartScene");
    controller<OperationDetails>("OperationDetails");
    controller<Credits>("Credits");
    controller<HelpScreen>("HelpScreen");
    controller<Panel>("Panel");
    controller<PauseMenu>("PauseMenu");
    controller<CFOPauseMenu>("CFOPauseMenu");
    controller<TutorialPopup>("TutorialPopup");
    controller<MissionWin>("MissionWin");
    controller<MissionFailed>("MissionFailed");
    controller<CFOMissionWin>("CFOMissionWin");
    controller<CFOMissionFailed>("CFOMissionFailed");
    controller<UpgradeTool>("UpgradeTool");
    tool<Cutter>("Cutter");
    tool<Lighter>("Lighter");
    tool<Cream>("Cream");
    tool<Stitch>("Stitch");
    tool<Tongs>("Tongs");
    tool<Vacuum>("Vacuum");
    tool<Scanner>("Scanner");
    tool<Injection>("Injection");
    tool<Chainsaw>("Chainsaw");
    tool<Battery>("Battery");
    injury<SmallCut>("SmallCut");
    injury<Incision>("Incision");
    injury<LargeCut>("LargeCut");
    injury<LargeCutScar>("LargeCutScar");
    injury<ClownNose>("ClownNose");
    injury<Burn>("Burn");
    injury<Burn>("BloodBurn");
    injury<BloodCut>("BloodSmallCut");
    injury<BloodCut>("BloodLargeCut");
    injury<Insertion>("Insertion");
    injury<SurfaceExtraction>("SurfaceExtraction");
    injury<SurfaceExtraction>("SurfaceExtractionStake");
    injury<SurfaceExtraction>("SurfaceExtractionGarlic");
    injury<UnderSurfaceExtraction>("UnderSurfaceExtraction");
    injury<UnderSurfaceExtraction>("UnderSurfaceExtractionSilver");
    injury<UnderSurfaceExtractionClown>("UnderSurfaceExtractionClown");
    injury<Bunny>("Bunny");
    injury<DeadDove>("DeadDove");
    injury<DeadWasp>("DeadWasp");
    injury<DeadSquid>("DeadSquid");
    injury<DeadCentipede>("DeadCentipede");
    for (const char* const* bone = BoneTransplant::kinds(); *bone; ++bone) injury<BoneTransplant>(*bone);
    injury<SkullCut>("SkullCut");
    injury<ReplaceSkull>("ReplaceSkull");
    injury<TypeAndTell>("TypeAndTell");
    injuryWithVelocity<Splatter>("Splatter");
    injuryWithVelocity<BloodSplatter>("BloodSplatter");
    injuryWithVelocity<BlueBloodSplatter>("BlueBloodSplatter");
    injury<Crab>("Crab");
    injury<Dove>("Dove");
    injury<Droplet>("Droplet");
    injury<InsectBoss>("InsectBoss");
    injury<Bat>("Bat");
    injury<Fire>("Fire");
    injuryWithVelocity<FireSplatter>("FireSplatter");
    injuryWithVelocity<SaltSplatter>("SaltSplatter");
    injuryWithVelocity<DoveSplatter>("DoveSplatter");
    injuryWithVelocity<FeatherSplatter>("FeatherSplatter");
    injury<Poison>("Poison");
    injuryWithVelocity<PoisonSplatter>("PoisonSplatter");
    injuryWithVelocity<InkSplatter>("InkSplatter");
    injuryWithVelocity<BarnacleSplatter>("BarnacleSplatter");
    injury<Barnacle>("Barnacle");
    injury<Squid>("Squid");
    injury<Fat>("Fat");
    injury<PeanutExtraction>("PeanutExtraction");
    injuryWithVelocity<PeanutSplatter>("PeanutSplatter");
    injury<SimonSays>("SimonSays");
    injury<Goo>("Goo");
    injury<ChickenLeg>("ChickenLeg");
    injuryWithVelocity<YellowSplatter>("YellowSplatter");
    injury<Snake>("Snake");
    injury<Dirt>("Dirt");
    injury<IncisionCoffin>("IncisionCoffin");
    injury<Bleed>("Bleed");
    injury<Centipede>("Centipede");
    injury<BigCentipede>("BigCentipede");
    injury<Eyelid>("Eyelid");
    injury<ExplosiveExtraction>("ExplosiveExtraction");
    injury<ExplosiveExtractionBomb>("ExplosiveExtractionBomb");
    injury<FireFly>("FireFly");
    injury<Tick>("Tick");
    injury<BigTick>("BigTick");
    injury<Puslet>("Puslet");
    injury<AmpPlug>("AmpPlug");
    injury<Transistor>("Transistor");
    injury<BrainPill>("BrainPill");
    injury<PoisonParticle>("PoisonParticle");
    injury<FireFlyEgg>("FireFlyEgg");
    injury<ExtractionFireFlyEggBroken>("ExtractionFireFlyEggBroken");
    injury<Lip>("Lip");
    injury<Unibrow>("Unibrow");
    injuryWithVelocity<FatSplatter>("FatSplatter");
}

}  // namespace as2
