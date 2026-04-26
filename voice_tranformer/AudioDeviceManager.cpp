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

ComPtr<IMMDevice> AudioDeviceManager::GetRenderDeviceByName(const std::wstring& targetName) {
    ComPtr<IMMDeviceCollection> pCollection;

    // 1. 활성화된(꽂혀있는) 모든 렌더(출력) 장치 목록 가져오기
    HRESULT hr = m_pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) return nullptr;

    UINT count = 0;
    pCollection->GetCount(&count);

    // 2. 장치 목록을 하나씩 순회
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> pDevice;
        if (FAILED(pCollection->Item(i, &pDevice))) continue;

        ComPtr<IPropertyStore> pProps;
        if (FAILED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) continue;

        // 3. 장치의 "친숙한 이름(Friendly Name)" 가져오기
        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);

        if (SUCCEEDED(hr) && varName.pwszVal != nullptr) {
            std::wstring deviceName = varName.pwszVal;
            PropVariantClear(&varName); // 메모리 누수 방지

            // 4. 장치 이름에 우리가 찾는 단어(targetName)가 포함되어 있는지 확인
            if (deviceName.find(targetName) != std::wstring::npos) {
                // 기존 함수 재활용: 찾은 장치의 이름과 포맷 출력
                PrintDeviceName(pDevice.Get(), L"선택된 가상 스피커(Render)");
                CheckFormatSupport(pDevice.Get(), L"가상 스피커");
                return pDevice; // 찾았으면 장치 반환!
            }
        }
        else {
            PropVariantClear(&varName);
        }
    }

    // 끝까지 다 돌았는데 못 찾은 경우
    std::wcout << L"[실패] '" << targetName << L"' 이름이 포함된 출력 장치를 찾을 수 없습니다." << std::endl;
    return nullptr;
}