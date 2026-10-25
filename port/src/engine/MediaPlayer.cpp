#include "engine/MediaPlayer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "miniaudio.h"
#include "platform/Assets.h"
#include "platform/AudioFile.h"
#include "platform/MovieDecoder.h"
#include "platform/Platform.h"

namespace {

// AS_AUDIO_AVOID=<text> (automated runs): play on the first output device
// whose name does not contain it (not the player's own headset), else the
// default one.
void chooseDevice(ma_engine_config& cfg) {
    const char* avoid = std::getenv("AS_AUDIO_AVOID");
    if (!avoid || !*avoid) return;
    static ma_context context;
    static ma_device_id id;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) return;
    ma_device_info* infos = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context, &infos, &count, nullptr, nullptr) != MA_SUCCESS) return;
    for (ma_uint32 i = 0; i < count; ++i) {
        if (std::strstr(infos[i].name, avoid)) continue;
        id = infos[i].id;
        cfg.pContext = &context;
        cfg.pPlaybackDeviceID = &id;
        platform::debugTrace("audio device %s", infos[i].name);
        return;
    }
}

// OALSoundEngine: one audio engine for the whole game.
ma_engine* engine() {
    static ma_engine instance;
    static bool ok = [] {
        ma_engine_config cfg = ma_engine_config_init();
        chooseDevice(cfg);
        if (ma_engine_init(&cfg, &instance) != MA_SUCCESS) {
            SDL_Log("No audio device; sound is disabled.");
            return false;
        }
        return true;
    }();
    return ok ? &instance : nullptr;
}

float gMasterSoundVolume = 1.0f;  // OALSoundEngine.masterSoundVolume

// The whole sound decoded: Apple's CAF (platform/AudioFile: PCM, IMA4),
// miniaudio's own decoders (MP3, WAV), else the platform's (AAC in .m4a).
bool decodeWhole(const std::string& name, const std::vector<unsigned char>& file, std::vector<float>& out,
                 int& channels, int& sampleRate) {
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (platform::decodeCaf(file.data(), file.size(), out, channels, sampleRate)) return true;
    ma_uint64 frames = 0;
    void* pcm = nullptr;
    if (ma_decode_memory(file.data(), file.size(), &cfg, &frames, &pcm) == MA_SUCCESS) {
        const float* samples = static_cast<const float*>(pcm);
        out.assign(samples, samples + frames * cfg.channels);
        ma_free(pcm, nullptr);
        channels = static_cast<int>(cfg.channels);
        sampleRate = static_cast<int>(cfg.sampleRate);
        return true;
    }
    if (platform::decodeAudioFile(assets::path(name), out, channels, sampleRate)) return true;
    SDL_Log("Cannot decode %s", name.c_str());
    return false;
}

}  // namespace

bool decodeSound(const std::string& name, std::vector<float>& pcm, int& channels, int& sampleRate) {
    std::vector<unsigned char> file;
    return assets::read(name, file) && decodeWhole(name, file, pcm, channels, sampleRate);
}

// ------------------------------------------------------------- StreamedPcm

namespace {

// A sound played while a worker thread decodes it (the platform's slow
// decoders: AAC on Android, piece by piece): a miniaudio data source over
// the samples decoded so far. Reading past them before the end gives
// silence (the decoding runs well ahead of the playing); the end, and so
// a loop's turn, comes only once all are decoded. Its length is unknown
// till then.
struct StreamedPcm {
    ma_data_source_base base;  // (first: miniaudio's handle is its address)
    std::mutex lock;
    std::condition_variable formatKnown;
    std::vector<float> pcm;
    int channels = 0, rate = 0;
    bool done = false, failed = false;
    std::atomic<bool> cancel{false};
    ma_uint64 cursor = 0;  // in frames (the mixer's thread)
    std::thread worker;
    bool baseReady = false;

    ~StreamedPcm() {
        cancel = true;
        if (worker.joinable()) worker.join();
        if (baseReady) ma_data_source_uninit(&base);
    }

    static ma_result read(ma_data_source* ds, void* out, ma_uint64 frameCount, ma_uint64* framesRead) {
        auto* s = reinterpret_cast<StreamedPcm*>(ds);
        std::lock_guard<std::mutex> g(s->lock);
        const ma_uint64 have = s->pcm.size() / s->channels;
        const ma_uint64 n = std::min(frameCount, have > s->cursor ? have - s->cursor : 0);
        float* f = static_cast<float*>(out);
        std::memcpy(f, s->pcm.data() + s->cursor * s->channels, static_cast<size_t>(n * s->channels) * sizeof(float));
        s->cursor += n;
        if (s->done) {
            if (framesRead) *framesRead = n;
            return n == 0 ? MA_AT_END : MA_SUCCESS;
        }
        // (not decoded yet: silence, the cursor kept)
        std::memset(f + n * s->channels, 0, static_cast<size_t>((frameCount - n) * s->channels) * sizeof(float));
        if (framesRead) *framesRead = frameCount;
        return MA_SUCCESS;
    }
    static ma_result seek(ma_data_source* ds, ma_uint64 frame) {
        auto* s = reinterpret_cast<StreamedPcm*>(ds);
        std::lock_guard<std::mutex> g(s->lock);
        s->cursor = frame;
        return MA_SUCCESS;
    }
    static ma_result format(ma_data_source* ds, ma_format* f, ma_uint32* channels, ma_uint32* rate, ma_channel*, size_t) {
        auto* s = reinterpret_cast<StreamedPcm*>(ds);
        if (f) *f = ma_format_f32;
        if (channels) *channels = static_cast<ma_uint32>(s->channels);
        if (rate) *rate = static_cast<ma_uint32>(s->rate);
        return MA_SUCCESS;
    }
    static ma_result cursorOf(ma_data_source* ds, ma_uint64* at) {
        auto* s = reinterpret_cast<StreamedPcm*>(ds);
        std::lock_guard<std::mutex> g(s->lock);
        *at = s->cursor;
        return MA_SUCCESS;
    }
    static ma_result lengthOf(ma_data_source* ds, ma_uint64* frames) {
        auto* s = reinterpret_cast<StreamedPcm*>(ds);
        std::lock_guard<std::mutex> g(s->lock);
        if (!s->done) return MA_NOT_IMPLEMENTED;  // (not known yet)
        *frames = s->pcm.size() / s->channels;
        return MA_SUCCESS;
    }

    // Starts the decoding; true once its format is known (it waits for that
    // only), false if it gives none.
    bool start(SoundEffectTrack::Producer produce) {
        static ma_data_source_vtable vtable = {read, seek, format, cursorOf, lengthOf, nullptr, 0};
        worker = std::thread([this, produce = std::move(produce)] {
            const bool ok = produce(
                [this](int c, int r) {
                    std::lock_guard<std::mutex> g(lock);
                    channels = c;
                    rate = r;
                    formatKnown.notify_all();
                },
                [this](const float* p, size_t n) {
                    std::lock_guard<std::mutex> g(lock);
                    pcm.insert(pcm.end(), p, p + n);
                    return !cancel.load();
                });
            std::lock_guard<std::mutex> g(lock);
            done = true;
            failed = !ok || channels <= 0;
            formatKnown.notify_all();
            // (debug: for test scripts)
            platform::debugTrace("sound streamed: %zu frames, %d channels, %d Hz", channels > 0 ? pcm.size() / channels : 0,
                                 channels, rate);
        });
        std::unique_lock<std::mutex> g(lock);
        formatKnown.wait(g, [this] { return channels > 0 || done; });
        if (channels <= 0 || rate <= 0) return false;
        ma_data_source_config cfg = ma_data_source_config_init();
        cfg.vtable = &vtable;
        baseReady = ma_data_source_init(&cfg, &base) == MA_SUCCESS;
        return baseReady;
    }
};

}  // namespace

// ---------------------------------------------------------------- MediaTrack

// All miniaudio state of one track. Heap-allocated once and never moved:
// the sound reads from the buffer/decoder by address.
struct MediaTrack::Audio {
    ma_sound sound;
    bool soundReady = false;
    ma_audio_buffer buffer;  // sound effects: fully decoded PCM
    bool bufferReady = false;
    std::vector<float> pcm;
    std::shared_ptr<const std::vector<int16_t>> shared;  // (initShared)
    ma_decoder decoder;  // music: streamed from the file bytes
    bool decoderReady = false;
    std::vector<unsigned char> file;
    std::unique_ptr<StreamedPcm> streamed;  // played as the platform decodes it
    int pcmChannels = 0, pcmRate = 0;

    ~Audio() {
        if (soundReady) ma_sound_uninit(&sound);  // before what it reads from
        if (bufferReady) ma_audio_buffer_uninit(&buffer);
        if (decoderReady) ma_decoder_uninit(&decoder);
        streamed.reset();  // (its decoding stopped)
    }
};

MediaTrack::MediaTrack(bool soundEffect) : isSoundEffect(soundEffect) {}
MediaTrack::~MediaTrack() = default;

void MediaTrack::applyGain() {
    if (!audio) return;
    float scale = isSoundEffect ? gMasterSoundVolume : 1.0f;
    ma_sound_set_volume(&audio->sound, muted ? 0.0f : volume / 100.0f * gain * scale);
}

void MediaTrack::setGain(float g) {
    gain = g;
    applyGain();
}

void MediaTrack::setPitch(float p) {
    if (audio) ma_sound_set_pitch(&audio->sound, p);
}

void MediaTrack::setVolume(int v) {
    volume = v;
    applyGain();
}

void MediaTrack::setReplayTimes(int n) {
    // Repeats loop in the mixer, sample-exact (no gap between plays); the
    // last play is let run out (update()).
    replayTimes = n;
    if (audio) ma_sound_set_looping(&audio->sound, n == -1 || n > 1 ? MA_TRUE : MA_FALSE);
}

void MediaTrack::setIsEnable(bool e) {
    enabled = e;
    if (!e && mediaState == kPlaying) stop();
}

void MediaTrack::setIsMute(bool m) {
    muted = m;
    applyGain();
}

void MediaTrack::play() {
    if (!enabled || !audio) return;
    ma_sound_stop(&audio->sound);
    ma_sound_seek_to_pcm_frame(&audio->sound, 0);
    playsLeft = replayTimes;
    ma_sound_set_looping(&audio->sound, replayTimes == -1 || replayTimes > 1 ? MA_TRUE : MA_FALSE);
    if (ma_engine* e = engine()) startMs = static_cast<double>(ma_engine_get_time_in_milliseconds(e));
    ma_sound_start(&audio->sound);
    mediaState = kPlaying;
}

void MediaTrack::stop() {
    if (audio) {
        ma_sound_stop(&audio->sound);
        ma_sound_seek_to_pcm_frame(&audio->sound, 0);
    }
    mediaState = kStopped;
}

void MediaTrack::pause() {
    if (mediaState != kPlaying) return;
    if (audio) ma_sound_stop(&audio->sound);
    if (ma_engine* e = engine()) pausedMs = static_cast<double>(ma_engine_get_time_in_milliseconds(e));
    mediaState = kPaused;
}

void MediaTrack::resume() {
    if (mediaState != kPaused) return;
    if (audio) ma_sound_start(&audio->sound);
    if (ma_engine* e = engine()) startMs += static_cast<double>(ma_engine_get_time_in_milliseconds(e)) - pausedMs;
    mediaState = kPlaying;
}

std::string MediaTrack::debugState() const {
    if (!audio) return "no audio";
    ma_uint64 cursor = 0, length = 0;
    ma_sound_get_cursor_in_pcm_frames(&audio->sound, &cursor);
    ma_sound_get_length_in_pcm_frames(&audio->sound, &length);
    ma_engine* e = engine();
    double elapsed = e ? (static_cast<double>(ma_engine_get_time_in_milliseconds(e)) - startMs) / 1000 : -1;
    char text[160];
    std::snprintf(text, sizeof text, "at end %d, playing %d, cursor %llu of %llu, %.3f s since play, plays %d/%d",
                  ma_sound_at_end(&audio->sound), ma_sound_is_playing(&audio->sound),
                  static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(length), elapsed, playsLeft,
                  replayTimes);
    return text;
}

int MediaTrack::getMediaState() const {
    // Sound effects reported their OpenAL source state: a finished sound is
    // stopped. ma_sound_is_playing() only says whether start()/stop() was
    // called, not whether playback reached the end (it stays true forever
    // after a start() with no matching stop(), even long after a one-shot
    // sound's own data ran out) — ma_sound_at_end() is miniaudio's own call
    // for that, and is what onSoundComplete and a non-looping sound's
    // "done playing" state both need.
    if (mediaState == kPlaying && audio && replayTimes != -1 && playsLeft <= 1) {
        if (ma_sound_at_end(&audio->sound)) return kStopped;
        // Seen in practice: a voice stops being mixed (so it never reaches
        // its end) without anything of ours telling it to. A real-time
        // timeout well past how long the sound could legitimately still be
        // going (so it never cuts one off early) catches that case too — an
        // onSoundComplete this late is better than a sound (and whatever it
        // gates: the next script line, the next loop of a track) stuck for
        // good.
        float length = 0;
        ma_engine* e = engine();
        if (e && ma_sound_get_length_in_seconds(&audio->sound, &length) == MA_SUCCESS && length > 0) {
            double elapsed = (static_cast<double>(ma_engine_get_time_in_milliseconds(e)) - startMs) / 1000;
            if (elapsed > length * std::max(1, replayTimes) * 3 + 2) return kStopped;
        }
    }
    return mediaState;
}

void MediaTrack::update() {
    if (mediaState != kPlaying || !audio || replayTimes <= 1 || playsLeft <= 1) return;
    // In its last play: no more looping, it ends there.
    ma_engine* e = engine();
    float length = 0;
    if (!e || ma_sound_get_length_in_seconds(&audio->sound, &length) != MA_SUCCESS || length <= 0) return;
    double played = (static_cast<double>(ma_engine_get_time_in_milliseconds(e)) - startMs) / 1000;
    if (played >= (replayTimes - 1) * static_cast<double>(length)) {
        playsLeft = 1;
        ma_sound_set_looping(&audio->sound, MA_FALSE);
    }
}

// ---------------------------------------------------------- SoundEffectTrack

SoundEffectTrack* SoundEffectTrack::initWithName(const std::string& name) {
    ma_engine* e = engine();
    std::vector<unsigned char> file;
    if (!e || !assets::read(name, file)) return nullptr;
    std::vector<float> data;
    int channels = 0, rate = 0;
    if (!decodeWhole(name, file, data, channels, rate)) return nullptr;
    return initWithPcm(std::move(data), channels, rate);
}

SoundEffectTrack* SoundEffectTrack::initWithPcm(std::vector<float> pcm, int channels, int sampleRate) {
    ma_engine* e = engine();
    if (!e || channels <= 0) return nullptr;
    auto audio = std::make_unique<Audio>();
    audio->pcm = std::move(pcm);
    ma_uint64 frames = audio->pcm.size() / channels;
    ma_audio_buffer_config bc =
        ma_audio_buffer_config_init(ma_format_f32, static_cast<ma_uint32>(channels), frames, audio->pcm.data(), nullptr);
    bc.sampleRate = static_cast<ma_uint32>(sampleRate);
    if (ma_audio_buffer_init(&bc, &audio->buffer) != MA_SUCCESS) return nullptr;
    audio->bufferReady = true;
    if (ma_sound_init_from_data_source(e, &audio->buffer, 0, nullptr, &audio->sound) != MA_SUCCESS) return nullptr;
    audio->soundReady = true;

    SoundEffectTrack* t = new SoundEffectTrack;
    t->audio = std::move(audio);
    t->applyGain();
    return t;
}

SoundEffectTrack* SoundEffectTrack::initShared(std::shared_ptr<const std::vector<int16_t>> pcm, int channels,
                                               int sampleRate) {
    ma_engine* e = engine();
    if (!e || channels <= 0 || !pcm) return nullptr;
    auto audio = std::make_unique<Audio>();
    audio->shared = std::move(pcm);
    ma_uint64 frames = audio->shared->size() / channels;
    ma_audio_buffer_config bc = ma_audio_buffer_config_init(ma_format_s16, static_cast<ma_uint32>(channels), frames,
                                                            audio->shared->data(), nullptr);
    bc.sampleRate = static_cast<ma_uint32>(sampleRate);
    if (ma_audio_buffer_init(&bc, &audio->buffer) != MA_SUCCESS) return nullptr;
    audio->bufferReady = true;
    if (ma_sound_init_from_data_source(e, &audio->buffer, 0, nullptr, &audio->sound) != MA_SUCCESS) return nullptr;
    audio->soundReady = true;

    SoundEffectTrack* t = new SoundEffectTrack;
    t->audio = std::move(audio);
    t->applyGain();
    return t;
}

SoundEffectTrack* SoundEffectTrack::initStreamed(Producer produce) {
    ma_engine* e = engine();
    if (!e) return nullptr;
    auto audio = std::make_unique<Audio>();
    audio->streamed = std::make_unique<StreamedPcm>();
    if (!audio->streamed->start(std::move(produce))) return nullptr;
    if (ma_sound_init_from_data_source(e, &audio->streamed->base, 0, nullptr, &audio->sound) != MA_SUCCESS) return nullptr;
    audio->soundReady = true;
    SoundEffectTrack* t = new SoundEffectTrack;
    t->audio = std::move(audio);
    t->applyGain();
    return t;
}

// ------------------------------------------------------ BackgroundMusicTrack

BackgroundMusicTrack* BackgroundMusicTrack::initWithName(const std::string& name) {
    BackgroundMusicTrack* t = new BackgroundMusicTrack;  // silent if anything below fails
    ma_engine* e = engine();
    auto audio = std::make_unique<Audio>();
    if (!e || !assets::read(name, audio->file)) return t;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_memory(audio->file.data(), audio->file.size(), &cfg, &audio->decoder) == MA_SUCCESS) {
        audio->decoderReady = true;  // streamed
        if (ma_sound_init_from_data_source(e, &audio->decoder, 0, nullptr, &audio->sound) != MA_SUCCESS) return t;
    } else if (platform::decodeCaf(audio->file.data(), audio->file.size(), audio->pcm, audio->pcmChannels, audio->pcmRate)) {
        // Apple's CAF (AS3's music): decoded whole (quick).
        audio->file.clear();
        ma_audio_buffer_config bc = ma_audio_buffer_config_init(ma_format_f32, static_cast<ma_uint32>(audio->pcmChannels),
                                                                audio->pcm.size() / audio->pcmChannels, audio->pcm.data(),
                                                                nullptr);
        bc.sampleRate = static_cast<ma_uint32>(audio->pcmRate);
        if (ma_audio_buffer_init(&bc, &audio->buffer) != MA_SUCCESS) return t;
        audio->bufferReady = true;
        if (ma_sound_init_from_data_source(e, &audio->buffer, 0, nullptr, &audio->sound) != MA_SUCCESS) return t;
    } else {
        // A format only the platform reads (AAC): played as it decodes.
        audio->file.clear();
        audio->streamed = std::make_unique<StreamedPcm>();
        const std::string path = assets::path(name);
        if (!audio->streamed->start([path](const platform::AudioFormatFn& f, const platform::AudioSamplesFn& s) {
                return platform::decodeAudioFileStreamed(path, f, s);
            }))
            return t;
        if (ma_sound_init_from_data_source(e, &audio->streamed->base, 0, nullptr, &audio->sound) != MA_SUCCESS) return t;
    }
    audio->soundReady = true;
    t->audio = std::move(audio);
    t->setReplayTimes(-1);  // AVAudioPlayer numberOfLoops = -1
    t->applyGain();
    return t;
}

void BackgroundMusicTrack::play() {
    if (muted) return;
    MediaTrack::play();
}

// --------------------------------------------------------------- MediaPlayer

SoundEffectTrack* MediaPlayer::allocSoundEffectTrack(const std::string& name, const std::string& subKey) {
    std::string k = key(name, subKey);
    auto it = mediaTrackList.find(k);
    if (it != mediaTrackList.end()) return static_cast<SoundEffectTrack*>(it->second);
    SoundEffectTrack* t = SoundEffectTrack::initWithName(name);
    if (!t) return nullptr;
    t->setVolume(100);
    t->setIsMute(soundEffectsMuted);
    t->setIsEnable(soundEffectsEnabled);
    mediaTrackList[k] = t;  // the list keeps the +1 reference
    return t;
}

SoundEffectTrack* MediaPlayer::allocSoundEffectTrackWithReplayTimes(const std::string& name,
                                                                    const std::string& subKey, int replayTimes) {
    SoundEffectTrack* t = allocSoundEffectTrack(name, subKey);
    if (t) t->setReplayTimes(replayTimes);
    return t;
}

SoundEffectTrack* MediaPlayer::allocSoundEffectTrackWithVolumeReplayTimes(const std::string& name,
                                                                          const std::string& subKey, int volume,
                                                                          int replayTimes) {
    SoundEffectTrack* t = allocSoundEffectTrack(name, subKey);
    if (t) {
        t->setVolume(volume);
        t->setReplayTimes(replayTimes);
    }
    return t;
}

BackgroundMusicTrack* MediaPlayer::allocBackgroundMusicTrack(const std::string& name, const std::string& subKey) {
    releaseBackgroundMusicTrack();
    backgroundMusicKey = key(name, subKey);
    backgroundMusicTrack = BackgroundMusicTrack::initWithName(name);
    backgroundMusicTrack->setIsMute(musicMuted);
    backgroundMusicTrack->setIsEnable(musicEnabled);
    auto it = mediaTrackList.find(backgroundMusicKey);
    if (it != mediaTrackList.end()) releaseRef(it->second);
    mediaTrackList[backgroundMusicKey] = retainRef(backgroundMusicTrack);
    return backgroundMusicTrack;
}

MediaTrack* MediaPlayer::getTrack(const std::string& name, const std::string& subKey) const {
    auto it = mediaTrackList.find(key(name, subKey));
    return it == mediaTrackList.end() ? nullptr : it->second;
}

void MediaPlayer::playTrack(const std::string& name, const std::string& subKey) {
    if (MediaTrack* t = getTrack(name, subKey)) t->play();
}
void MediaPlayer::stopTrack(const std::string& name, const std::string& subKey) {
    if (MediaTrack* t = getTrack(name, subKey)) t->stop();
}
void MediaPlayer::releaseBackgroundMusicTrack() {
    if (!backgroundMusicTrack) return;
    auto it = mediaTrackList.find(backgroundMusicKey);
    if (it != mediaTrackList.end()) {
        releaseRef(it->second);
        mediaTrackList.erase(it);
    }
    releaseRef(backgroundMusicTrack);
    backgroundMusicTrack = nullptr;
    backgroundMusicKey.clear();
}

void MediaPlayer::releaseAllMediaTrack() {
    releaseBackgroundMusicTrack();
    for (auto& kv : mediaTrackList) releaseRef(kv.second);
    mediaTrackList.clear();
}

float MediaPlayer::getSoundEffectMasterVolume() const { return gMasterSoundVolume; }

// Master volume is set as 0..100 and kept (and reported) as 0..1.
void MediaPlayer::setSoundEffectMasterVolume(int v) {
    gMasterSoundVolume = v / 100.0f;
    for (auto& kv : mediaTrackList)
        if (kv.first != backgroundMusicKey) kv.second->setVolume(kv.second->getVolume());
}

void MediaPlayer::setBackgroundMusicMasterVolume(int v) {
    if (backgroundMusicTrack) backgroundMusicTrack->setVolume(v);
}

void MediaPlayer::setIsAllSoundEffectTrackEnable(bool e) {
    for (auto& kv : mediaTrackList)
        if (kv.first != backgroundMusicKey) kv.second->setIsEnable(e);
    soundEffectsEnabled = e;
}

void MediaPlayer::setIsBackgroundMusicTrackEnable(bool e) {
    if (backgroundMusicTrack) backgroundMusicTrack->setIsEnable(e);
    musicEnabled = e;
}

void MediaPlayer::update() {
    for (auto& kv : mediaTrackList) kv.second->update();
}
