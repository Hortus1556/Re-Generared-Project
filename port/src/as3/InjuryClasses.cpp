#include "as3/InjuryClasses.h"

#include "as3/BoneInjuries.h"
#include "as3/CutInjuries.h"
#include "as3/Extractions.h"
#include "as3/ParticleInjuries.h"
#include "as3/SkinInjuries.h"

namespace mt {
namespace injuries {

InjuryState* make(int base) {
    switch (base) {
    case kArtery: return new Artery;                          // case 1
    case kBoil: return new Boil;                              // case 4
    case kCementDrop: return new CementDrop;                  // case 6
    case kIncision: return new Incision;                      // case 9
    case kLargeCut: return new LargeCut;                      // case 10
    case kPassiveParticle: return new PassiveParticle;        // case 0xe
    case kPoisonArea: return new PoisonArea;                  // case 0xf
    case kSoakableParticle: return new SoakableParticle;      // case 0x13
    case kTransplant: return new Transplant;                  // case 0x14
    case kUnderSurfaceExtraction: return new UnderSurfaceExtraction;  // case 0x15
    case kBrokenBone: return new BrokenBone;                  // case 0x16
    case kBrokenVein: return new BrokenVein;                  // case 0x17
    case kFire: return new Fire;                              // case 0x18
    case kSurfaceExtraction: return new SurfaceExtraction;    // case 0x1a
    case kSnakeCut: return new SnakeCut;                      // case 0x20
    case kSuperPoisonArea: return new SuperPoisonArea;        // case 0x21
    case kSuperPoisonScab: return new SuperPoisonScab;        // case 0x22
    case kFrostBite: return new FrostBite;                    // case 0x23
    case kSpikeBase: return new SpikeBase;                    // case 0x24
    default: return nullptr;  // (Burn 5, SmallCut 0x11: the base as it is)
    }
}

}  // namespace injuries
}  // namespace mt
