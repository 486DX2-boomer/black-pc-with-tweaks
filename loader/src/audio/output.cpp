// XAudio2 (xaudio2_9.dll ships with Windows 10, loaded at runtime): a mastering voice and one float
// source voice that the mixer keeps QUEUED blocks ahead of the device.
#include "audio/output.h"

#include "common.h"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace audio_out {
namespace {

constexpr uint32_t QUEUED = 6, RING = QUEUED + 1; // 60 ms ahead of the device

IXAudio2 *g_xa;
IXAudio2MasteringVoice *g_master;
IXAudio2SourceVoice *g_src;
float g_ring[RING][BLOCK * 2];
uint32_t g_next;
FILE *g_wav;
uint32_t g_wav_frames;

void wav_header()
{
    uint32_t data = g_wav_frames * 4;
    struct {
        char riff[4]; uint32_t size; char wave[4], fmt[4]; uint32_t fmt_size;
        uint16_t tag, ch; uint32_t rate, bytes_per_s; uint16_t align, bits;
        char data[4]; uint32_t data_size;
    } h = {{'R', 'I', 'F', 'F'}, 36 + data, {'W', 'A', 'V', 'E'}, {'f', 'm', 't', ' '}, 16, 1, 2, RATE, RATE * 4, 4, 16,
           {'d', 'a', 't', 'a'}, data};
    fseek(g_wav, 0, SEEK_SET);
    fwrite(&h, sizeof h, 1, g_wav);
    fseek(g_wav, 0, SEEK_END);
}

} // namespace

bool open()
{
    typedef HRESULT(WINAPI * PFN_Create)(IXAudio2 **, UINT32, XAUDIO2_PROCESSOR);
    HMODULE m = LoadLibraryW(L"xaudio2_9.dll");
    if (!m) m = LoadLibraryW(L"xaudio2_8.dll");
    auto create = m ? (PFN_Create)GetProcAddress(m, "XAudio2Create") : nullptr;
    if (!create) {
        logf("[audio] XAudio2 not available: no sound");
        return false;
    }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); // XAudio2 needs COM on the creating thread
    HRESULT hr = create(&g_xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (SUCCEEDED(hr)) hr = g_xa->CreateMasteringVoice(&g_master, 2, RATE);
    if (SUCCEEDED(hr) && g_cfg.background) g_master->SetVolume(0.0f); // tests: keep the timing, not the sound
    if (!g_cfg.wav_path.empty() && !_wfopen_s(&g_wav, g_cfg.wav_path.c_str(), L"wb")) wav_header();
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wf.nChannels = 2;
    wf.nSamplesPerSec = RATE;
    wf.wBitsPerSample = 32;
    wf.nBlockAlign = 8;
    wf.nAvgBytesPerSec = RATE * 8;
    if (SUCCEEDED(hr)) hr = g_xa->CreateSourceVoice(&g_src, &wf, XAUDIO2_VOICE_NOPITCH);
    if (SUCCEEDED(hr)) hr = g_src->Start();
    if (FAILED(hr)) {
        logf("[audio] XAudio2 setup failed 0x%08lx: no sound", hr);
        g_src = nullptr;
        return false;
    }
    logf("[audio] XAudio2 output: %u Hz stereo, %u ms blocks, %u queued", RATE, BLOCK * 1000 / RATE, QUEUED);
    return true;
}

uint32_t blocks_wanted()
{
    if (!g_src) return 0;
    XAUDIO2_VOICE_STATE st;
    g_src->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    static uint32_t underruns, submitted_before;
    if (st.BuffersQueued == 0 && g_next != submitted_before) { // the device ran dry since the last check: a gap
        if (++underruns <= 5 || underruns % 100 == 0) logf("[audio] output underrun #%u (the mixer fell behind)", underruns);
    }
    submitted_before = g_next;
    return st.BuffersQueued < QUEUED ? QUEUED - st.BuffersQueued : 0;
}

void submit(const float *lr)
{
    if (g_wav) {
        int16_t pcm[BLOCK * 2];
        for (uint32_t i = 0; i < BLOCK * 2; i++) pcm[i] = (int16_t)(std::max(-1.0f, std::min(1.0f, lr[i])) * 32767.0f);
        fwrite(pcm, sizeof pcm, 1, g_wav);
        g_wav_frames += BLOCK;
        if (g_wav_frames % (RATE / 2) == 0) wav_header(); // keep the header valid if the process is killed
    }
    if (!g_src) return;
    float *b = g_ring[g_next];
    g_next = (g_next + 1) % RING;
    memcpy(b, lr, sizeof g_ring[0]);
    XAUDIO2_BUFFER xb = {};
    xb.AudioBytes = sizeof g_ring[0];
    xb.pAudioData = (const BYTE *)b;
    g_src->SubmitSourceBuffer(&xb);
}

} // namespace audio_out
