#include <iostream>
#include <locale>

#pragma comment(lib, "Mmdevapi.lib")

#include "AudioDeviceManager.h"
#include "AudioEngine.h"
#include "AudioProcessor.h"

int main() {
    std::wcout.imbue(std::locale("korean"));

    // 1. 시스템 초기화 (COM)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;

    std::wcout << L"--- WASAPI 음성 변조 프로그램 ---" << std::endl;

    // 2. 장치 관리자 생성 및 마이크/스피커 획득
    AudioDeviceManager deviceManager;
    if (deviceManager.Initialize()) {
        auto pCaptureDevice = deviceManager.GetDefaultCaptureDevice();
        auto pRenderDevice = deviceManager.GetDefaultRenderDevice();

        if (pCaptureDevice && pRenderDevice) {
            // 3. 프로세서(변환기) 및 엔진 생성
            //NoiseGateProcessor converter(-40.0f, 10.0f, 300.0f);
            EchoProcessor converter(200.0f, 0.3f);
            AudioEngine engine;

            // 4. 엔진에 장치와 프로세서를 넣고 가동
            engine.Run(pCaptureDevice.Get(), pRenderDevice.Get(), &converter);
        }
    }

    // 5. 종료 처리
    CoUninitialize();
    std::wcout << L"프로그램을 종료합니다." << std::endl;
    return 0;
}