#include "AudioDeviceManager.h"
#include <iostream>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>

AudioDeviceManager::AudioDeviceManager() {}
AudioDeviceManager::~AudioDeviceManager() {}

bool AudioDeviceManager::Initialize() {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_pEnumerator));
    return SUCCEEDED(hr);
}

ComPtr<IMMDevice> AudioDeviceManager::GetDefaultCaptureDevice() {
    ComPtr<IMMDevice> pDevice;
    if (SUCCEEDED(m_pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice))) {
        PrintDeviceName(pDevice.Get(), L"기본 마이크(Capture)");
        CheckFormatSupport(pDevice.Get(), L"기본 마이크");
        return pDevice;
    }
    return nullptr;
}

ComPtr<IMMDevice> AudioDeviceManager::GetDefaultRenderDevice() {
    ComPtr<IMMDevice> pDevice;
    if (SUCCEEDED(m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) {
        PrintDeviceName(pDevice.Get(), L"기본 스피커(Render)");
        CheckFormatSupport(pDevice.Get(), L"기본 스피커");
        return pDevice;
    }
    return nullptr;
}

void AudioDeviceManager::PrintDeviceName(IMMDevice* pDevice, const std::wstring& deviceType) {
    ComPtr<IPropertyStore> pProps;
    if (FAILED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) return;

    PROPVARIANT varName;
    PropVariantInit(&varName);
    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
        std::wcout << deviceType << L": " << varName.pwszVal << std::endl;
    }
    PropVariantClear(&varName);
}

bool AudioDeviceManager::CheckFormatSupport(IMMDevice* pDevice, const std::wstring& deviceName) {
    // (기존의 CheckFormatSupport 내부 코드와 100% 동일하므로 생략 없이 그대로 붙여넣으시면 됩니다)
    ComPtr<IAudioClient> pAudioClient;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &pAudioClient);
    if (FAILED(hr)) return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 48000;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    WAVEFORMATEX* pClosestMatch = nullptr;
    hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wfx, &pClosestMatch);

    if (hr == S_OK) {
        std::wcout << deviceName << L" -> [성공] 48kHz, 32-bit Float, Stereo 호환!" << std::endl;
        return true;
    }
    else if (hr == S_FALSE) {
        std::wcout << deviceName << L" -> [경고] 제안된 포맷: "
            << pClosestMatch->nSamplesPerSec << L"Hz, "
            << pClosestMatch->wBitsPerSample << L"비트, "
            << pClosestMatch->nChannels << L"채널" << std::endl;
        CoTaskMemFree(pClosestMatch);
        return false;
    }
    return false;
}