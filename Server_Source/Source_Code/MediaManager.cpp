#include "MediaManager.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// Quét tất cả camera có sẵn trên hệ thống - Simple version without DirectShow
std::string ScanAvailableCameras() {
    json cameraList = json::array();
    
    try {
        for (int i = 0; i < 5; i++) {  // Limit to 5 cameras max
            cv::VideoCapture testCap;
            testCap.open(i, cv::CAP_DSHOW);
            
            if (testCap.isOpened()) {
                json camInfo;
                camInfo["index"] = i;
                camInfo["name"] = "Camera " + std::to_string(i);
                
                // Get resolution
                int width = (int)testCap.get(cv::CAP_PROP_FRAME_WIDTH);
                int height = (int)testCap.get(cv::CAP_PROP_FRAME_HEIGHT);
                camInfo["resolution"] = std::to_string(width) + "x" + std::to_string(height);
                
                cameraList.push_back(camInfo);
                testCap.release();
                
                // Small delay between camera checks
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                break;  // Stop scanning when no more cameras found
            }
        }
    } catch (...) {
        std::cout << "[Camera] Scan error" << std::endl;
    }
    
    std::cout << "[Camera] Found " << cameraList.size() << " camera(s)" << std::endl;
    return cameraList.dump();
}

// Scan for available microphones using waveIn API (safer than DirectShow)
std::string ScanAvailableMicrophones() {
    json micList = json::array();
    
    try {
        UINT numDevices = waveInGetNumDevs();
        std::cout << "[Mic] Found " << numDevices << " input device(s)" << std::endl;
        
        for (UINT i = 0; i < numDevices && i < 10; i++) {
            WAVEINCAPSW caps;  // Use wide version
            if (waveInGetDevCapsW(i, &caps, sizeof(WAVEINCAPSW)) == MMSYSERR_NOERROR) {
                json micInfo;
                micInfo["index"] = (int)i;
                
                // Convert wide string to UTF-8
                char nameBuffer[128];
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, 
                    nameBuffer, sizeof(nameBuffer), NULL, NULL);
                micInfo["name"] = std::string(nameBuffer);
                
                micInfo["channels"] = (int)caps.wChannels;
                micList.push_back(micInfo);
            }
        }
    } catch (...) {
        std::cout << "[Mic] Scan error" << std::endl;
    }
    
    return micList.dump();
}

// GDI+ Encoder
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// Chụp màn hình (Full Virtual Screen)
std::string CaptureScreenBase64() {
    int x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x1, y1, SRCCOPY);

    Bitmap bitmap(hBitmap, NULL);
    IStream* pStream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);

    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);
    bitmap.Save(pStream, &clsid, NULL);

    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
    STATSTG stg;
    pStream->Stat(&stg, STATFLAG_NONAME);

    DWORD size = (DWORD)stg.cbSize.QuadPart;
    std::vector<unsigned char> buffer(size);
    ULONG bytesRead = 0;
    pStream->Read(&buffer[0], size, &bytesRead);

    pStream->Release();
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return Base64Encode(&buffer[0], size);
}

// Live Stream Webcam
void WebcamThreadFunc(server* s, websocketpp::connection_hdl hdl, int cameraIndex) {
    cv::VideoCapture cap(cameraIndex, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        g_IsStreaming = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;
    std::vector<uchar> buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 50 };

    while (g_IsStreaming) {
        cap >> frame;
        if (frame.empty()) break;

        cv::imencode(".jpg", frame, buf, params);
        std::string jpg_b64 = Base64Encode(buf.data(), (unsigned int)buf.size());

        try {
            json j;
            j["type"] = "CAM_FRAME";
            j["data"] = jpg_b64;
            s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
        }
        catch (...) {
            g_IsStreaming = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    cap.release();
}

// Record Video with custom duration
void RecordVideoThread(server* s, websocketpp::connection_hdl hdl, int duration, int cameraIndex) {
    cv::VideoCapture cap(cameraIndex, cv::CAP_DSHOW);
    if (!cap.isOpened()) return;

    int width = 640;
    int height = 480;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    
    // Send CAM_READY notification when camera is ready
    try {
        json j_ready;
        j_ready["type"] = "CAM_READY";
        s->send(hdl, j_ready.dump(), websocketpp::frame::opcode::text);
    }
    catch (...) {}

    std::string filename = "server_rec_temp.avi";
    cv::VideoWriter writer;
    writer.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 20.0, cv::Size(width, height), true);

    cv::Mat frame;
    int totalFrames = duration * 20; // 20 FPS
    for (int i = 0; i < totalFrames; i++) {
        cap >> frame;
        if (frame.empty()) break;
        writer.write(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    writer.release();
    cap.release();

    std::vector<unsigned char> fileData = ReadFileToBuffer(filename);
    if (!fileData.empty()) {
        std::string b64 = Base64Encode(fileData.data(), (unsigned int)fileData.size());
        json j;
        j["type"] = "RECORD_RESULT";
        j["data"] = b64;
        try {
            s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
        }
        catch (...) {}
    }
}

void StartWebcamStream(server* s, websocketpp::connection_hdl hdl, int cameraIndex) {
    if (!g_IsStreaming) {
        g_IsStreaming = true;
        g_WebcamThread = new std::thread(WebcamThreadFunc, s, hdl, cameraIndex);
        g_WebcamThread->detach();
    }
}

void StartRecordVideo(server* s, websocketpp::connection_hdl hdl, int duration, int cameraIndex) {
    std::thread t(RecordVideoThread, s, hdl, duration, cameraIndex);
    t.detach();
}

// =====================================================
// MICROPHONE FUNCTIONS
// =====================================================

// Record audio using Windows waveIn API
void RecordAudioThread(server* s, websocketpp::connection_hdl hdl, int duration, int micIndex) {
    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 1;
    const int BITS_PER_SAMPLE = 16;
    const int BUFFER_SIZE = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8) * duration;
    
    HWAVEIN hWaveIn;
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS_PER_SAMPLE;
    wfx.nBlockAlign = (CHANNELS * BITS_PER_SAMPLE) / 8;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * wfx.nBlockAlign;
    wfx.cbSize = 0;
    
    MMRESULT result = waveInOpen(&hWaveIn, micIndex, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        try {
            json j;
            j["type"] = "ACTION_RESULT";
            j["msg"] = "Failed to open microphone";
            s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
        } catch (...) {}
        return;
    }
    
    std::vector<char> buffer(BUFFER_SIZE);
    WAVEHDR waveHdr;
    waveHdr.lpData = buffer.data();
    waveHdr.dwBufferLength = BUFFER_SIZE;
    waveHdr.dwFlags = 0;
    
    waveInPrepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    waveInAddBuffer(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    waveInStart(hWaveIn);
    
    // Wait for recording to complete
    while (!(waveHdr.dwFlags & WHDR_DONE)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    waveInStop(hWaveIn);
    waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    waveInClose(hWaveIn);
    
    // Create WAV file in memory
    std::vector<unsigned char> wavData;
    
    // WAV header
    wavData.insert(wavData.end(), {'R', 'I', 'F', 'F'});
    uint32_t fileSize = 36 + waveHdr.dwBytesRecorded;
    wavData.push_back(fileSize & 0xFF);
    wavData.push_back((fileSize >> 8) & 0xFF);
    wavData.push_back((fileSize >> 16) & 0xFF);
    wavData.push_back((fileSize >> 24) & 0xFF);
    
    wavData.insert(wavData.end(), {'W', 'A', 'V', 'E'});
    wavData.insert(wavData.end(), {'f', 'm', 't', ' '});
    
    uint32_t fmtSize = 16;
    wavData.push_back(fmtSize & 0xFF);
    wavData.push_back((fmtSize >> 8) & 0xFF);
    wavData.push_back((fmtSize >> 16) & 0xFF);
    wavData.push_back((fmtSize >> 24) & 0xFF);
    
    wavData.push_back(1); wavData.push_back(0); // PCM
    wavData.push_back(CHANNELS); wavData.push_back(0);
    
    wavData.push_back(SAMPLE_RATE & 0xFF);
    wavData.push_back((SAMPLE_RATE >> 8) & 0xFF);
    wavData.push_back((SAMPLE_RATE >> 16) & 0xFF);
    wavData.push_back((SAMPLE_RATE >> 24) & 0xFF);
    
    uint32_t byteRate = SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8;
    wavData.push_back(byteRate & 0xFF);
    wavData.push_back((byteRate >> 8) & 0xFF);
    wavData.push_back((byteRate >> 16) & 0xFF);
    wavData.push_back((byteRate >> 24) & 0xFF);
    
    uint16_t blockAlign = CHANNELS * BITS_PER_SAMPLE / 8;
    wavData.push_back(blockAlign & 0xFF);
    wavData.push_back((blockAlign >> 8) & 0xFF);
    
    wavData.push_back(BITS_PER_SAMPLE); wavData.push_back(0);
    
    wavData.insert(wavData.end(), {'d', 'a', 't', 'a'});
    wavData.push_back(waveHdr.dwBytesRecorded & 0xFF);
    wavData.push_back((waveHdr.dwBytesRecorded >> 8) & 0xFF);
    wavData.push_back((waveHdr.dwBytesRecorded >> 16) & 0xFF);
    wavData.push_back((waveHdr.dwBytesRecorded >> 24) & 0xFF);
    
    // Add audio data
    for (DWORD i = 0; i < waveHdr.dwBytesRecorded; i++) {
        wavData.push_back(buffer[i]);
    }
    
    // Send as base64
    std::string b64 = Base64Encode(wavData.data(), (unsigned int)wavData.size());
    
    try {
        json j;
        j["type"] = "AUDIO_RESULT";
        j["data"] = b64;
        s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
    } catch (...) {}
}

void RecordMicAudio(server* s, websocketpp::connection_hdl hdl, int duration, int micIndex) {
    std::thread t(RecordAudioThread, s, hdl, duration, micIndex);
    t.detach();
}

// Live audio streaming thread
void LiveAudioThread(server* s, websocketpp::connection_hdl hdl, int micIndex) {
    const int SAMPLE_RATE = 16000;  // Lower sample rate for streaming
    const int CHANNELS = 1;
    const int BITS_PER_SAMPLE = 16;
    const int CHUNK_DURATION_MS = 200;  // Send chunks every 200ms
    const int BUFFER_SIZE = (SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8) * CHUNK_DURATION_MS) / 1000;
    
    HWAVEIN hWaveIn;
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS_PER_SAMPLE;
    wfx.nBlockAlign = (CHANNELS * BITS_PER_SAMPLE) / 8;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * wfx.nBlockAlign;
    wfx.cbSize = 0;
    
    // Use WAVE_MAPPER if micIndex is invalid
    UINT deviceId = (micIndex >= 0) ? (UINT)micIndex : WAVE_MAPPER;
    
    MMRESULT result = waveInOpen(&hWaveIn, deviceId, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        std::cout << "[Audio] Failed to open mic, error: " << result << std::endl;
        try {
            json j;
            j["type"] = "ACTION_RESULT";
            j["msg"] = "Failed to open microphone for streaming (error: " + std::to_string(result) + ")";
            s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
        } catch (...) {}
        g_IsStreamingAudio = false;
        return;
    }
    
    // Use 2 buffers for double buffering
    const int NUM_BUFFERS = 2;
    char* buffers[NUM_BUFFERS];
    WAVEHDR waveHdrs[NUM_BUFFERS];
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = new char[BUFFER_SIZE];
        memset(buffers[i], 0, BUFFER_SIZE);
        memset(&waveHdrs[i], 0, sizeof(WAVEHDR));
        waveHdrs[i].lpData = buffers[i];
        waveHdrs[i].dwBufferLength = BUFFER_SIZE;
        waveHdrs[i].dwFlags = 0;
        waveHdrs[i].dwLoops = 0;
        
        result = waveInPrepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cout << "[Audio] Failed to prepare header " << i << ", error: " << result << std::endl;
        }
        
        result = waveInAddBuffer(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cout << "[Audio] Failed to add buffer " << i << ", error: " << result << std::endl;
        }
    }
    
    result = waveInStart(hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cout << "[Audio] Failed to start recording, error: " << result << std::endl;
        for (int i = 0; i < NUM_BUFFERS; i++) {
            waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
            delete[] buffers[i];
        }
        waveInClose(hWaveIn);
        g_IsStreamingAudio = false;
        return;
    }
    
    std::cout << "[Audio] Started live streaming from mic " << micIndex << std::endl;
    std::cout << "[Audio] Format: " << SAMPLE_RATE << "Hz, " << CHANNELS << "ch, " << BITS_PER_SAMPLE << "bit" << std::endl;
    std::cout << "[Audio] Buffer size: " << BUFFER_SIZE << " bytes (" << CHUNK_DURATION_MS << "ms chunks)" << std::endl;
    
    // Send start notification
    try {
        json j;
        j["type"] = "ACTION_RESULT";
        j["msg"] = "Live audio streaming started";
        s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
    } catch (...) {}
    
    int currentBuffer = 0;
    int chunkCount = 0;
    
    while (g_IsStreamingAudio) {
        // Wait for current buffer to be done
        int waitMs = 0;
        while (!(waveHdrs[currentBuffer].dwFlags & WHDR_DONE)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waitMs += 10;
            
            if (!g_IsStreamingAudio) break;
            
            // Timeout after 500ms
            if (waitMs >= 500) {
                std::cout << "[Audio] Buffer " << currentBuffer << " timeout" << std::endl;
                break;
            }
        }
        
        if (!g_IsStreamingAudio) break;
        
        // Check if buffer has data
        if (waveHdrs[currentBuffer].dwFlags & WHDR_DONE) {
            DWORD bytesRecorded = waveHdrs[currentBuffer].dwBytesRecorded;
            
            if (bytesRecorded > 0) {
                std::string b64 = Base64Encode(
                    (unsigned char*)buffers[currentBuffer], 
                    bytesRecorded
                );
                
                try {
                    json j;
                    j["type"] = "AUDIO_CHUNK";
                    j["data"] = b64;
                    j["sampleRate"] = SAMPLE_RATE;
                    j["channels"] = CHANNELS;
                    j["bitsPerSample"] = BITS_PER_SAMPLE;
                    s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
                    
                    chunkCount++;
                    if (chunkCount % 5 == 0) {
                        std::cout << "[Audio] Sent " << chunkCount << " chunks" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "[Audio] Send error: " << e.what() << std::endl;
                    g_IsStreamingAudio = false;
                    break;
                } catch (...) {
                    std::cout << "[Audio] Unknown send error" << std::endl;
                    g_IsStreamingAudio = false;
                    break;
                }
            }
            
            // Reset buffer and add back to queue
            // No need to unprepare/prepare again - just clear flags and re-add
            waveHdrs[currentBuffer].dwBytesRecorded = 0;
            waveHdrs[currentBuffer].dwFlags &= ~WHDR_DONE;  // Clear DONE flag only
            
            result = waveInAddBuffer(hWaveIn, &waveHdrs[currentBuffer], sizeof(WAVEHDR));
            if (result != MMSYSERR_NOERROR) {
                std::cout << "[Audio] Failed to requeue buffer " << currentBuffer << ", error: " << result << std::endl;
                // Try to recover by resetting and re-adding all buffers
                waveInReset(hWaveIn);
                for (int i = 0; i < NUM_BUFFERS; i++) {
                    waveHdrs[i].dwBytesRecorded = 0;
                    waveHdrs[i].dwFlags = 0;
                    waveInAddBuffer(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
                }
                waveInStart(hWaveIn);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        currentBuffer = (currentBuffer + 1) % NUM_BUFFERS;
    }
    
    std::cout << "[Audio] Stopping stream, sent " << chunkCount << " chunks total" << std::endl;
    
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveInUnprepareHeader(hWaveIn, &waveHdrs[i], sizeof(WAVEHDR));
        delete[] buffers[i];
    }
    
    waveInClose(hWaveIn);
    
    std::cout << "[Audio] Stream closed" << std::endl;
}

void StartMicStream(server* s, websocketpp::connection_hdl hdl, int micIndex) {
    if (!g_IsStreamingAudio) {
        g_IsStreamingAudio = true;
        g_AudioThread = new std::thread(LiveAudioThread, s, hdl, micIndex);
        g_AudioThread->detach();
    }
}

void StopMicStream() {
    g_IsStreamingAudio = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}