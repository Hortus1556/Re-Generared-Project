#include "surgeon/SoundBank.h"

#include <algorithm>

#include "engine/MediaPlayer.h"
#include "engine/VectorArt.h"

SoundBank::SoundBank(VectorArt& art, std::vector<Sound> sounds) : art(art) {
    for (Sound& s : sounds) entries.push_back({std::move(s)});
}

SoundBank::~SoundBank() {
    for (Entry& e : entries) {
        if (e.track) e.track->stop();
        releaseRef(e.track);
    }
}

SoundBank::Entry* SoundBank::find(const std::string& name) {
    for (Entry& e : entries)
        if (e.sound.name == name) return &e;
    return nullptr;
}

const SoundBank::Entry* SoundBank::find(const std::string& name) const {
    for (const Entry& e : entries)
        if (e.sound.name == name) return &e;
    return nullptr;
}

bool SoundBank::allowed(const Entry& e) const {
    switch (e.sound.group) {
    case kEffect: return effects;
    case kMusic: return music;
    default: return effects && music;
    }
}

void SoundBank::play(const std::string& name, int times) {
    if (scripts.play) {
        scripts.play(name, times);
        return;
    }
    Entry* e = find(name);
    if (!e || !allowed(*e) || playing(name)) return;
    if (!e->track) {
        const VectorArt::Pcm* pcm = art.sound(e->sound.linkage);
        if (!pcm) return;
        e->track = SoundEffectTrack::initShared(pcm->samples, pcm->channels, pcm->rate);
        if (!e->track) return;
    }
    e->track->setVolume(e->sound.volume);
    e->track->setReplayTimes(std::max(1, times));
    e->track->play();
}

void SoundBank::stop(const std::string& name) {
    if (scripts.stop) {
        scripts.stop(name);
        return;
    }
    Entry* e = find(name);
    if (e && e->track) e->track->stop();
}

void SoundBank::stopAll() {
    for (Entry& e : entries)
        if (e.track) e.track->stop();
}

bool SoundBank::playing(const std::string& name) const {
    const Entry* e = find(name);
    return e && e->track && e->track->getMediaState() == MediaTrack::kPlaying;
}

void SoundBank::setEnabled(bool m, bool fx) {
    if (scripts.enable) {
        scripts.enable(m, fx);
        return;
    }
    music = m;
    effects = fx;
    for (Entry& e : entries)
        if (e.track && !allowed(e)) e.track->stop();
}

void SoundBank::update() {
    for (Entry& e : entries)
        if (e.track) e.track->update();
}
