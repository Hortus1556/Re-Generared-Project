#include "platform/MovieDecoder.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// clang-format off
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
// clang-format on

#include <cstdio>
#include <cstring>
#include <string>

namespace platform {

namespace {

template <class T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

// One reader per stream keeps audio and video from waiting on each other.
IMFSourceReader* openReader(const std::wstring& path, bool video) {
    IMFAttributes* attrs = nullptr;
    if (FAILED(MFCreateAttributes(&attrs, 1))) return nullptr;
    if (video) attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    IMFSourceReader* reader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), attrs, &reader);
    release(attrs);
    return SUCCEEDED(hr) ? reader : nullptr;
}

bool selectOutput(IMFSourceReader* reader, DWORD stream, const GUID& major, const GUID& subtype) {
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(stream, TRUE);
    IMFMediaType* type = nullptr;
    if (FAILED(MFCreateMediaType(&type))) return false;
    type->SetGUID(MF_MT_MAJOR_TYPE, major);
    type->SetGUID(MF_MT_SUBTYPE, subtype);
    HRESULT hr = reader->SetCurrentMediaType(stream, nullptr, type);
    release(type);
    return SUCCEEDED(hr);
}

// Every sample of the reader's (selected, float) audio stream.
bool readAllAudio(IMFSourceReader* r, std::vector<float>& pcm, int& channels, int& sampleRate) {
    const DWORD stream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    IMFMediaType* type = nullptr;
    if (FAILED(r->GetCurrentMediaType(stream, &type))) return false;
    channels = static_cast<int>(MFGetAttributeUINT32(type, MF_MT_AUDIO_NUM_CHANNELS, 0));
    sampleRate = static_cast<int>(MFGetAttributeUINT32(type, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
    release(type);
    pcm.clear();
    for (;;) {
        DWORD flags = 0;
        IMFSample* sample = nullptr;
        if (FAILED(r->ReadSample(stream, 0, nullptr, &flags, nullptr, &sample))) break;
        if (sample) {
            IMFMediaBuffer* buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
                BYTE* data = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&data, nullptr, &len))) {
                    const float* f = reinterpret_cast<const float*>(data);
                    pcm.insert(pcm.end(), f, f + len / sizeof(float));
                    buf->Unlock();
                }
                release(buf);
            }
            release(sample);
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
    }
    return channels > 0 && sampleRate > 0 && !pcm.empty();
}

// Apple's gapless info (the iTunSMPB tag of an .m4a): the encoder's
// priming samples at the start and the real sample count. Media
// Foundation's AAC decoding keeps the priming and the end padding, which a
// loop would play as a gap.
bool gaplessInfo(const std::string& path, unsigned& delay, unsigned long long& count) {
    FILE* f = _wfopen(widen(path).c_str(), L"rb");
    if (!f) return false;
    std::string bytes;
    char buf[65536];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) bytes.append(buf, n);
    std::fclose(f);
    size_t at = bytes.find("iTunSMPB");
    if (at == std::string::npos) return false;
    // Its text: " 00000000 <delay> <padding> <count> ...", hex.
    size_t text = bytes.find(" 00000000 ", at);
    if (text == std::string::npos || text > at + 64) return false;
    unsigned zero = 0, padding = 0;
    return std::sscanf(bytes.c_str() + text, " %x %x %x %llx", &zero, &delay, &padding, &count) == 4 && count > 0;
}

}  // namespace

bool decodeAudioFile(const std::string& path, std::vector<float>& pcm, int& channels, int& sampleRate) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool ok = false;
    if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) {
        if (IMFSourceReader* r = openReader(widen(path), false)) {
            ok = selectOutput(r, static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), MFMediaType_Audio,
                              MFAudioFormat_Float) &&
                 readAllAudio(r, pcm, channels, sampleRate);
            r->Release();
        }
        MFShutdown();
    }
    if (SUCCEEDED(co)) CoUninitialize();
    unsigned delay = 0;
    unsigned long long count = 0;
    if (ok && channels > 0 && gaplessInfo(path, delay, count)) {
        // Only the real samples (sample-exact loops).
        size_t frames = pcm.size() / static_cast<size_t>(channels);
        if (delay + count <= frames) {
            pcm.erase(pcm.begin() + static_cast<long long>((delay + count) * channels), pcm.end());
            pcm.erase(pcm.begin(), pcm.begin() + static_cast<long long>(delay * channels));
        }
    }
    return ok;
}

bool decodeAudioFileStreamed(const std::string& path, const AudioFormatFn& format, const AudioSamplesFn& samples) {
    std::vector<float> pcm;
    int channels = 0, rate = 0;
    if (!decodeAudioFile(path, pcm, channels, rate)) return false;
    format(channels, rate);
    samples(pcm.data(), pcm.size());
    return true;
}

struct MovieDecoder::Impl {
    bool comStarted = false, mfStarted = false;
    IMFSourceReader* video = nullptr;
    IMFSourceReader* audio = nullptr;
    int width = 0, height = 0;
    LONG stride = 0;

    ~Impl() {
        release(video);
        release(audio);
        if (mfStarted) MFShutdown();
        if (comStarted) CoUninitialize();
    }
};

std::unique_ptr<MovieDecoder> MovieDecoder::open(const std::string& path) {
    auto impl = std::make_unique<Impl>();
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    impl->comStarted = SUCCEEDED(co);
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) return nullptr;
    impl->mfStarted = true;

    std::wstring wpath = widen(path);
    impl->video = openReader(wpath, true);
    if (!impl->video ||
        !selectOutput(impl->video, static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), MFMediaType_Video,
                      MFVideoFormat_RGB32))
        return nullptr;
    IMFMediaType* type = nullptr;
    if (FAILED(impl->video->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &type)))
        return nullptr;
    UINT32 w = 0, h = 0;
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h);
    UINT32 stride = 0;
    impl->stride = SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)) ? static_cast<LONG>(stride)
                                                                             : static_cast<LONG>(w * 4);
    release(type);
    impl->width = static_cast<int>(w);
    impl->height = static_cast<int>(h);
    if (w == 0 || h == 0) return nullptr;

    impl->audio = openReader(wpath, false);
    if (impl->audio &&
        !selectOutput(impl->audio, static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), MFMediaType_Audio,
                      MFAudioFormat_Float))
        release(impl->audio);
    return std::unique_ptr<MovieDecoder>(new MovieDecoder(std::move(impl)));
}

MovieDecoder::MovieDecoder(std::unique_ptr<Impl> i) : impl(std::move(i)) {}
MovieDecoder::~MovieDecoder() = default;

bool MovieDecoder::readAudio(std::vector<float>& pcm, int& channels, int& sampleRate) {
    return impl->audio && readAllAudio(impl->audio, pcm, channels, sampleRate);
}

bool MovieDecoder::streamAudio(const AudioFormatFn& format, const AudioSamplesFn& samples) {
    std::vector<float> pcm;
    int channels = 0, rate = 0;
    if (!readAudio(pcm, channels, rate)) return false;
    format(channels, rate);
    samples(pcm.data(), pcm.size());
    return true;
}

bool MovieDecoder::nextFrame(std::vector<unsigned char>& bgra, int& width, int& height, double& seconds) {
    const DWORD stream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    for (;;) {
        DWORD flags = 0;
        LONGLONG time = 0;
        IMFSample* sample = nullptr;
        if (FAILED(impl->video->ReadSample(stream, 0, nullptr, &flags, &time, &sample))) return false;
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            release(sample);
            return false;
        }
        if (!sample) continue;  // a stream tick or format note
        width = impl->width;
        height = impl->height;
        seconds = time / 1e7;
        bgra.resize(static_cast<size_t>(width) * height * 4);
        IMFMediaBuffer* buf = nullptr;
        bool ok = false;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
            IMF2DBuffer* buf2d = nullptr;
            BYTE* scan0 = nullptr;
            LONG pitch = 0;
            if (SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(&buf2d))) && SUCCEEDED(buf2d->Lock2D(&scan0, &pitch))) {
                for (int y = 0; y < height; ++y)
                    std::memcpy(bgra.data() + static_cast<size_t>(y) * width * 4, scan0 + static_cast<ptrdiff_t>(y) * pitch,
                                static_cast<size_t>(width) * 4);
                buf2d->Unlock2D();
                ok = true;
            } else {
                BYTE* data = nullptr;
                if (SUCCEEDED(buf->Lock(&data, nullptr, nullptr))) {
                    LONG s = impl->stride;
                    BYTE* row0 = s < 0 ? data + static_cast<ptrdiff_t>(-s) * (height - 1) : data;
                    for (int y = 0; y < height; ++y)
                        std::memcpy(bgra.data() + static_cast<size_t>(y) * width * 4, row0 + static_cast<ptrdiff_t>(y) * s,
                                    static_cast<size_t>(width) * 4);
                    buf->Unlock();
                    ok = true;
                }
            }
            release(buf2d);
            release(buf);
        }
        release(sample);
        return ok;
    }
}

}  // namespace platform


#elif defined(__ANDROID__)  // Android: the NDK's media codecs

#include <SDL3/SDL.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaDataSource.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace platform {

namespace {

// A file read whole (SDL reads the APK's assets by their relative path),
// handed to the extractor as its data.
struct Bytes {
    std::vector<unsigned char> data;
};

ssize_t readAt(void* user, off64_t offset, void* buffer, size_t size) {
    const Bytes* b = static_cast<const Bytes*>(user);
    if (offset < 0) return -1;
    // At the end: no bytes, as a file gives (-1 there, which the NDK
    // documents as the end, made the MP4 reader fail .m4a files whose index
    // comes after the audio: it reads on past it while parsing).
    if (static_cast<size_t>(offset) >= b->data.size()) return 0;
    size_t n = std::min(size, b->data.size() - static_cast<size_t>(offset));
    std::memcpy(buffer, b->data.data() + offset, n);
    return static_cast<ssize_t>(n);
}

ssize_t sizeOf(void* user) { return static_cast<ssize_t>(static_cast<const Bytes*>(user)->data.size()); }

void closeSource(void*) {}

// Apple's gapless info (the iTunSMPB tag of an .m4a): the encoder's
// priming samples at the start and the real sample count.
bool gaplessInfo(const Bytes& b, unsigned& delay, unsigned long long& count) {
    std::string bytes(b.data.begin(), b.data.end());
    size_t at = bytes.find("iTunSMPB");
    if (at == std::string::npos) return false;
    size_t text = bytes.find(" 00000000 ", at);
    if (text == std::string::npos || text > at + 64) return false;
    unsigned zero = 0, padding = 0;
    return std::sscanf(bytes.c_str() + text, " %x %x %x %llx", &zero, &delay, &padding, &count) == 4 && count > 0;
}

}  // namespace

bool decodeAudioFileStreamed(const std::string& path, const AudioFormatFn& format, const AudioSamplesFn& samples) {
    Bytes bytes;
    size_t n = 0;
    void* data = SDL_LoadFile(path.c_str(), &n);
    if (!data) return false;
    bytes.data.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + n);
    SDL_free(data);
    // Apple's gapless info: the encoder's priming left out at the start, and
    // nothing after the real samples (sample-exact loops). The NDK's codecs
    // give the priming as it is (trimming is the players' job).
    unsigned delay = 0;
    unsigned long long count = 0;
    const bool gapless = gaplessInfo(bytes, delay, count);

    AMediaDataSource* source = AMediaDataSource_new();
    AMediaDataSource_setUserdata(source, &bytes);
    AMediaDataSource_setReadAt(source, readAt);
    AMediaDataSource_setGetSize(source, sizeOf);
    AMediaDataSource_setClose(source, closeSource);
    AMediaExtractor* ex = AMediaExtractor_new();
    int channels = 0, sampleRate = 0;
    AMediaCodec* codec = nullptr;
    if (AMediaExtractor_setDataSourceCustom(ex, source) == AMEDIA_OK) {
        // Its first audio track.
        size_t tracks = AMediaExtractor_getTrackCount(ex);
        for (size_t t = 0; t < tracks && !codec; ++t) {
            AMediaFormat* f = AMediaExtractor_getTrackFormat(ex, t);
            const char* mime = nullptr;
            if (AMediaFormat_getString(f, AMEDIAFORMAT_KEY_MIME, &mime) && mime && std::strncmp(mime, "audio/", 6) == 0) {
                AMediaExtractor_selectTrack(ex, t);
                codec = AMediaCodec_createDecoderByType(mime);
                if (codec && AMediaCodec_configure(codec, f, nullptr, nullptr, 0) != AMEDIA_OK) {
                    AMediaCodec_delete(codec);
                    codec = nullptr;
                }
                int32_t c = 0, r = 0;
                if (AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &c)) channels = c;
                if (AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_SAMPLE_RATE, &r)) sampleRate = r;
            }
            AMediaFormat_delete(f);
        }
    }
    bool any = false;
    if (codec && AMediaCodec_start(codec) == AMEDIA_OK) {
        // Samples in, PCM out, to the end of the stream. Every input buffer
        // free is filled at once, and the output is waited for only briefly
        // while there is input left.
        bool inputDone = false, outputDone = false, formatGiven = false, stopped = false;
        int32_t encoding = 2;  // AudioFormat.ENCODING_PCM_16BIT
        unsigned long long frame = 0;  // frames decoded so far (the priming counted)
        std::vector<float> piece;
        while (!outputDone && !stopped) {
            while (!inputDone) {
                ssize_t in = AMediaCodec_dequeueInputBuffer(codec, 0);
                if (in < 0) break;
                size_t cap = 0;
                uint8_t* buf = AMediaCodec_getInputBuffer(codec, static_cast<size_t>(in), &cap);
                ssize_t size = AMediaExtractor_readSampleData(ex, buf, cap);
                if (size < 0) {
                    AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(in), 0, 0, 0,
                                                 AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    inputDone = true;
                } else {
                    AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(in), 0, static_cast<size_t>(size),
                                                 static_cast<uint64_t>(AMediaExtractor_getSampleTime(ex)), 0);
                    AMediaExtractor_advance(ex);
                }
            }
            AMediaCodecBufferInfo info;
            ssize_t out = AMediaCodec_dequeueOutputBuffer(codec, &info, inputDone ? 5000 : 500);
            if (out == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                AMediaFormat* f = AMediaCodec_getOutputFormat(codec);
                int32_t c = 0, r = 0, e = 0;
                if (AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &c)) channels = c;
                if (AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_SAMPLE_RATE, &r)) sampleRate = r;
                if (AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_PCM_ENCODING, &e)) encoding = e;
                AMediaFormat_delete(f);
            } else if (out >= 0) {
                size_t cap = 0;
                uint8_t* buf = AMediaCodec_getOutputBuffer(codec, static_cast<size_t>(out), &cap);
                if (buf && info.size > 0 && channels > 0 && sampleRate > 0) {
                    if (!formatGiven) {
                        format(channels, sampleRate);
                        formatGiven = true;
                    }
                    const uint8_t* at = buf + info.offset;
                    piece.clear();
                    if (encoding == 4) {  // ENCODING_PCM_FLOAT
                        const float* f = reinterpret_cast<const float*>(at);
                        piece.assign(f, f + info.size / 4);
                    } else {
                        const int16_t* v = reinterpret_cast<const int16_t*>(at);
                        piece.resize(static_cast<size_t>(info.size / 2));
                        for (size_t k = 0; k < piece.size(); ++k) piece[k] = v[k] / 32768.0f;
                    }
                    // (the gapless trim: frames before `delay` and from
                    // `delay + count` on left out)
                    const unsigned long long frames = piece.size() / static_cast<size_t>(channels);
                    unsigned long long from = 0, to = frames;
                    if (gapless) {
                        from = frame < delay ? std::min<unsigned long long>(delay - frame, frames) : 0;
                        to = frame + frames > delay + count ? (delay + count > frame ? delay + count - frame : 0) : frames;
                    }
                    frame += frames;
                    if (to > from) {
                        any = true;
                        if (!samples(piece.data() + from * channels, static_cast<size_t>((to - from) * channels)))
                            stopped = true;
                    }
                }
                AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(out), false);
                if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) outputDone = true;
            }
        }
        AMediaCodec_stop(codec);
    }
    if (codec) AMediaCodec_delete(codec);
    AMediaExtractor_delete(ex);
    AMediaDataSource_delete(source);
    return any;
}

bool decodeAudioFile(const std::string& path, std::vector<float>& pcm, int& channels, int& sampleRate) {
    pcm.clear();
    return decodeAudioFileStreamed(
        path,
        [&](int c, int r) {
            channels = c;
            sampleRate = r;
        },
        [&](const float* p, size_t count) {
            pcm.insert(pcm.end(), p, p + count);
            return true;
        });
}

// Movies: the video track decoded by the NDK's codec into its buffers (no
// surface), each frame's YUV 4:2:0 turned into BGRA as Media Foundation gives
// it on Windows; the soundtrack read whole as a sound file's
// (decodeAudioFile: its first audio track). The frames' layout is the one
// the codec's output format describes (its "image-data", a MediaImage2: each
// plane's offset and steps, which is how the "flexible" format's buffers are
// read), else planar I420 (19) or semi-planar NV12 (21) by stride and slice
// height.
struct MovieDecoder::Impl {
    std::string path;
    Bytes bytes;  // (the extractor reads it: kept as long as it)
    AMediaDataSource* source = nullptr;
    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    bool inputDone = false, outputDone = false;
    // the frames' layout, from the codec's output format
    int width = 0, height = 0;        // shown (the crop)
    int cropLeft = 0, cropTop = 0;
    struct Plane {
        size_t offset = 0;
        int colInc = 1, rowInc = 0;  // bytes to the next sample across and down (chroma: per two pixels)
    };
    Plane plane[3];  // Y, U, V
    bool readable = false;

    ~Impl() {
        if (codec) {
            AMediaCodec_stop(codec);
            AMediaCodec_delete(codec);
        }
        if (extractor) AMediaExtractor_delete(extractor);
        if (source) AMediaDataSource_delete(source);
    }

    void readFormat(AMediaFormat* f) {
        int32_t w = 0, h = 0, v = 0, colorFormat = 0;
        AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_WIDTH, &w);
        AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_HEIGHT, &h);
        const int stride = AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_STRIDE, &v) && v > 0 ? v : w;
        const int slice = AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &v) && v > 0 ? v : h;
        AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat);
        // MediaImage2 (media/hardware/VideoAPI.h): type, planes, width,
        // height, bit depth, its allocation, then per plane offset, column
        // step, row step, subsampling across and down (uint32 each)
        void* image = nullptr;
        size_t imageSize = 0;
        readable = false;
        if (AMediaFormat_getBuffer(f, "image-data", &image, &imageSize) && imageSize >= 104) {
            uint32_t u[26];
            std::memcpy(u, image, sizeof u);
            if (u[0] == 1 && u[1] == 3 && u[4] == 8) {  // YUV, three planes, 8 bits
                for (int k = 0; k < 3; ++k) {
                    plane[k].offset = u[6 + k * 5];
                    plane[k].colInc = static_cast<int32_t>(u[7 + k * 5]);
                    plane[k].rowInc = static_cast<int32_t>(u[8 + k * 5]);
                }
                readable = true;
            }
        }
        if (!readable && (colorFormat == 19 || colorFormat == 21)) {
            const size_t ySize = static_cast<size_t>(stride) * slice;
            plane[0] = {0, 1, stride};
            if (colorFormat == 19) {  // I420: U then V, each a quarter
                plane[1] = {ySize, 1, stride / 2};
                plane[2] = {ySize + static_cast<size_t>(stride / 2) * (slice / 2), 1, stride / 2};
            } else {  // NV12: U and V interleaved
                plane[1] = {ySize, 2, stride};
                plane[2] = {ySize + 1, 2, stride};
            }
            readable = true;
        }
        int32_t l = 0, t = 0, r = w - 1, b = h - 1;
        if (AMediaFormat_getRect(f, AMEDIAFORMAT_KEY_DISPLAY_CROP, &l, &t, &r, &b)) {
            cropLeft = l;
            cropTop = t;
            width = r - l + 1;
            height = b - t + 1;
        } else {
            cropLeft = cropTop = 0;
            width = w;
            height = h;
        }
    }
};

namespace {

// One frame's YUV (BT.601, video range) as BGRA, top row first (in fixed
// point: every pixel of every frame, on a phone).
void yuvToBgra(const uint8_t* buf, size_t size, const MovieDecoder::Impl& m, std::vector<unsigned char>& bgra) {
    const int w = m.width, h = m.height;
    const auto& P = m.plane;
    auto clamp = [](int c) { return static_cast<unsigned char>(c < 0 ? 0 : c > 255 ? 255 : c); };
    bgra.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const int sy = y + m.cropTop;
        const uint8_t* yRow = buf + P[0].offset + static_cast<ptrdiff_t>(sy) * P[0].rowInc;
        const size_t uRow = P[1].offset + static_cast<size_t>(sy / 2) * P[1].rowInc;
        const size_t vRow = P[2].offset + static_cast<size_t>(sy / 2) * P[2].rowInc;
        unsigned char* px = bgra.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x, px += 4) {
            const int sx = x + m.cropLeft;
            const size_t ui = uRow + static_cast<size_t>(sx / 2) * P[1].colInc;
            const size_t vi = vRow + static_cast<size_t>(sx / 2) * P[2].colInc;
            if (ui >= size || vi >= size) continue;
            // (the coefficients times 1024)
            const int Y = 1192 * (yRow[static_cast<ptrdiff_t>(sx) * P[0].colInc] - 16), U = buf[ui] - 128, V = buf[vi] - 128;
            px[0] = clamp((Y + 2066 * U) >> 10);
            px[1] = clamp((Y - 400 * U - 833 * V) >> 10);
            px[2] = clamp((Y + 1634 * V) >> 10);
            px[3] = 255;
        }
    }
}

}  // namespace

std::unique_ptr<MovieDecoder> MovieDecoder::open(const std::string& path) {
    auto m = std::make_unique<Impl>();
    m->path = path;
    size_t n = 0;
    void* data = SDL_LoadFile(path.c_str(), &n);
    if (!data) return nullptr;
    m->bytes.data.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + n);
    SDL_free(data);
    m->source = AMediaDataSource_new();
    AMediaDataSource_setUserdata(m->source, &m->bytes);
    AMediaDataSource_setReadAt(m->source, readAt);
    AMediaDataSource_setGetSize(m->source, sizeOf);
    AMediaDataSource_setClose(m->source, closeSource);
    m->extractor = AMediaExtractor_new();
    if (AMediaExtractor_setDataSourceCustom(m->extractor, m->source) != AMEDIA_OK) return nullptr;
    // its first video track, decoded into buffers in a YUV 4:2:0 layout
    size_t tracks = AMediaExtractor_getTrackCount(m->extractor);
    for (size_t t = 0; t < tracks && !m->codec; ++t) {
        AMediaFormat* f = AMediaExtractor_getTrackFormat(m->extractor, t);
        const char* mime = nullptr;
        if (AMediaFormat_getString(f, AMEDIAFORMAT_KEY_MIME, &mime) && mime && std::strncmp(mime, "video/", 6) == 0) {
            AMediaExtractor_selectTrack(m->extractor, t);
            m->readFormat(f);
            AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F420888);  // COLOR_FormatYUV420Flexible
            m->codec = AMediaCodec_createDecoderByType(mime);
            if (m->codec && (AMediaCodec_configure(m->codec, f, nullptr, nullptr, 0) != AMEDIA_OK ||
                             AMediaCodec_start(m->codec) != AMEDIA_OK)) {
                AMediaCodec_delete(m->codec);
                m->codec = nullptr;
            }
        }
        AMediaFormat_delete(f);
    }
    if (!m->codec || m->width <= 0 || m->height <= 0) return nullptr;
    return std::unique_ptr<MovieDecoder>(new MovieDecoder(std::move(m)));
}

MovieDecoder::MovieDecoder(std::unique_ptr<Impl> i) : impl(std::move(i)) {}
MovieDecoder::~MovieDecoder() = default;

bool MovieDecoder::readAudio(std::vector<float>& pcm, int& channels, int& sampleRate) {
    return decodeAudioFile(impl->path, pcm, channels, sampleRate);
}

bool MovieDecoder::streamAudio(const AudioFormatFn& format, const AudioSamplesFn& samples) {
    return decodeAudioFileStreamed(impl->path, format, samples);
}

bool MovieDecoder::nextFrame(std::vector<unsigned char>& bgra, int& width, int& height, double& seconds) {
    Impl& m = *impl;
    while (!m.outputDone) {
        while (!m.inputDone) {  // (every input buffer free filled at once)
            ssize_t in = AMediaCodec_dequeueInputBuffer(m.codec, 0);
            if (in < 0) break;
            {
                size_t cap = 0;
                uint8_t* buf = AMediaCodec_getInputBuffer(m.codec, static_cast<size_t>(in), &cap);
                ssize_t size = AMediaExtractor_readSampleData(m.extractor, buf, cap);
                if (size < 0) {
                    AMediaCodec_queueInputBuffer(m.codec, static_cast<size_t>(in), 0, 0, 0,
                                                 AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    m.inputDone = true;
                } else {
                    AMediaCodec_queueInputBuffer(m.codec, static_cast<size_t>(in), 0, static_cast<size_t>(size),
                                                 static_cast<uint64_t>(AMediaExtractor_getSampleTime(m.extractor)), 0);
                    AMediaExtractor_advance(m.extractor);
                }
            }
        }
        AMediaCodecBufferInfo info;
        ssize_t out = AMediaCodec_dequeueOutputBuffer(m.codec, &info, m.inputDone ? 5000 : 1000);
        if (out == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* f = AMediaCodec_getOutputFormat(m.codec);
            m.readFormat(f);
            AMediaFormat_delete(f);
            if (!m.readable) {
                SDL_Log("Movie %s: the codec's frame layout is not read", m.path.c_str());
                m.outputDone = true;
            }
        } else if (out >= 0) {
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) m.outputDone = true;
            size_t cap = 0;
            uint8_t* buf = AMediaCodec_getOutputBuffer(m.codec, static_cast<size_t>(out), &cap);
            const bool frame = buf && info.size > 0;
            if (frame) {
                yuvToBgra(buf + info.offset, static_cast<size_t>(info.size), m, bgra);
                width = m.width;
                height = m.height;
                seconds = info.presentationTimeUs / 1e6;
            }
            AMediaCodec_releaseOutputBuffer(m.codec, static_cast<size_t>(out), false);
            if (frame) return true;
        }
    }
    return false;
}

}  // namespace platform

#else  // Linux (and anything else not yet given a real decoder): AAC and
       // movie decoding are not implemented here yet, unlike Windows's
       // Media Foundation and Android's NDK codecs above. MP3/WAV sounds
       // (miniaudio, AudioFile.cpp) are unaffected -- this only means a
       // game that needs .m4a music or a movie (AS2, AS3) will report
       // those as absent rather than play them; AS1 and Christmas use
       // neither. A real implementation would plug in here, e.g. with
       // ffmpeg's libavcodec, which this workspace already carries as a
       // tool dependency.

namespace platform {

bool decodeAudioFile(const std::string&, std::vector<float>&, int&, int&) { return false; }

bool decodeAudioFileStreamed(const std::string&, const AudioFormatFn&, const AudioSamplesFn&) { return false; }

struct MovieDecoder::Impl {};

std::unique_ptr<MovieDecoder> MovieDecoder::open(const std::string&) { return nullptr; }

MovieDecoder::MovieDecoder(std::unique_ptr<Impl> i) : impl(std::move(i)) {}
MovieDecoder::~MovieDecoder() = default;

bool MovieDecoder::readAudio(std::vector<float>&, int&, int&) { return false; }
bool MovieDecoder::streamAudio(const AudioFormatFn&, const AudioSamplesFn&) { return false; }
bool MovieDecoder::nextFrame(std::vector<unsigned char>&, int&, int&, double&) { return false; }

}  // namespace platform

#endif
