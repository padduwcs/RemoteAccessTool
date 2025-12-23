#include "MediaManager.h"
#include <mmsystem.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "winmm.lib")

// Suppress ALL output including DLL warnings (system-level stderr redirect)
class SuppressAllOutput {
public:
    SuppressAllOutput() {
        // Save original stderr
        old_stderr = _dup(_fileno(stderr));
        old_stdout = _dup(_fileno(stdout));
        
        // Redirect stderr and stdout to NUL
        FILE* null_file = nullptr;
        freopen_s(&null_file, "NUL", "w", stderr);
        freopen_s(&null_file, "NUL", "w", stdout);
    }
    
    ~SuppressAllOutput() {
        // Restore stderr and stdout
        _dup2(old_stderr, _fileno(stderr));
        _dup2(old_stdout, _fileno(stdout));
        _close(old_stderr);
        _close(old_stdout);
        
        // Re-sync C++ streams
        std::ios::sync_with_stdio(false);
        std::ios::sync_with_stdio(true);
    }
    
private:
    int old_stderr;
    int old_stdout;
};

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

// Chụp màn hình (Full Virtual Screen hoặc một màn hình cụ thể)
std::string CaptureScreenBase64(int monitorIndex) {
    int x1 = 0, y1 = 0, width = 0, height = 0;
    bool found = false;
    
    if (monitorIndex == -1) {
        // Chụp tất cả màn hình (Virtual Screen)
        x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
        width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        found = true;
        std::cout << "[Screenshot] Capturing all monitors: " << width << "x" << height << std::endl;
    } else {
        // Chụp một màn hình cụ thể
        DISPLAY_DEVICE dd;
        dd.cb = sizeof(DISPLAY_DEVICE);
        
        int currentMonitor = 0;
        for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
            if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
            
            if (currentMonitor == monitorIndex) {
                DEVMODE dm;
                dm.dmSize = sizeof(DEVMODE);
                if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                    x1 = dm.dmPosition.x;
                    y1 = dm.dmPosition.y;
                    width = dm.dmPelsWidth;
                    height = dm.dmPelsHeight;
                    found = true;
                    std::cout << "[Screenshot] Capturing monitor " << monitorIndex 
                              << " at (" << x1 << "," << y1 << ") "
                              << width << "x" << height << std::endl;
                    break;
                }
            }
            currentMonitor++;
        }
        
        // Fallback nếu không tìm thấy monitor
        if (!found) {
            std::cout << "[Screenshot] Monitor " << monitorIndex << " not found, using primary monitor" << std::endl;
            x1 = 0;
            y1 = 0;
            width = GetSystemMetrics(SM_CXSCREEN);
            height = GetSystemMetrics(SM_CYSCREEN);
            found = true;
        }
    }

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

    std::string filename = "server_rec_temp.mp4";
    cv::VideoWriter writer;
    std::string codecUsed = "unknown";
    
    // Suppress ALL output including DLL warnings during codec detection
    {
        SuppressAllOutput suppressAll;
        
        // Priority 1: Try avc1 first (most compatible, no warnings)
        writer.open(filename, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 20.0, cv::Size(width, height), true);
        if (writer.isOpened()) {
            codecUsed = "H.264 (AVC1)";
        }
        
        // Priority 2: Try H264
        if (!writer.isOpened()) {
            writer.open(filename, cv::VideoWriter::fourcc('H', '2', '6', '4'), 20.0, cv::Size(width, height), true);
            if (writer.isOpened()) {
                codecUsed = "H.264";
            }
        }
        
        // Priority 3: Try h264 (lowercase)
        if (!writer.isOpened()) {
            writer.open(filename, cv::VideoWriter::fourcc('h', '2', '6', '4'), 20.0, cv::Size(width, height), true);
            if (writer.isOpened()) {
                codecUsed = "H.264 (h264)";
            }
        }
        
        // Priority 4: Try X264 (may show warnings but works)
        if (!writer.isOpened()) {
            writer.open(filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), 20.0, cv::Size(width, height), true);
            if (writer.isOpened()) {
                codecUsed = "H.264 (X264)";
            }
        }
        
        // Fallback: Use MJPEG in AVI container
        if (!writer.isOpened()) {
            filename = "server_rec_temp.avi";
            writer.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 20.0, cv::Size(width, height), true);
            if (writer.isOpened()) {
                codecUsed = "MJPEG (AVI)";
            }
        }
    } // Destructor restores stderr/stdout - warnings completely hidden
    
    if (!writer.isOpened()) {
        std::cout << "[Record] ❌ No codec available!" << std::endl;
        cap.release();
        return;
    }
    
    // Only print final codec used - clean and simple
    std::cout << "[Record] ✅ " << codecUsed << " | " << width << "x" << height << " @ 20fps" << std::endl;

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

// =====================================================
// SCREEN MONITORING FUNCTIONS
// =====================================================

// Quét tất cả màn hình có sẵn trên hệ thống
std::string ScanAvailableMonitors() {
    json monitorList = json::array();
    
    try {
        DISPLAY_DEVICE dd;
        dd.cb = sizeof(DISPLAY_DEVICE);
        
        int monitorIndex = 0;
        for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
            if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
            
            DEVMODE dm;
            dm.dmSize = sizeof(DEVMODE);
            
            if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                json monInfo;
                monInfo["index"] = monitorIndex;
                
                // Convert device name to UTF-8
                char nameBuffer[128];
                WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)dd.DeviceName, -1, 
                    nameBuffer, sizeof(nameBuffer), NULL, NULL);
                
                monInfo["name"] = "Monitor " + std::to_string(monitorIndex + 1);
                monInfo["deviceName"] = std::string(nameBuffer);
                monInfo["resolution"] = std::to_string(dm.dmPelsWidth) + "x" + std::to_string(dm.dmPelsHeight);
                monInfo["position"] = std::to_string(dm.dmPosition.x) + "," + std::to_string(dm.dmPosition.y);
                monInfo["frequency"] = std::to_string(dm.dmDisplayFrequency) + "Hz";
                monInfo["isPrimary"] = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) ? true : false;
                
                monitorList.push_back(monInfo);
                monitorIndex++;
            }
        }
    } catch (...) {
        std::cout << "[Monitor] Scan error" << std::endl;
    }
    
    std::cout << "[Monitor] Found " << monitorList.size() << " monitor(s)" << std::endl;
    return monitorList.dump();
}

// Live Stream Screen
void ScreenStreamThreadFunc(server* s, websocketpp::connection_hdl hdl, int monitorIndex) {
    // Get monitor bounds
    int x1, y1, width, height;
    
    if (monitorIndex == -1) {
        // All monitors
        x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
        width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    } else {
        // Specific monitor
        DISPLAY_DEVICE dd;
        dd.cb = sizeof(DISPLAY_DEVICE);
        
        int currentMonitor = 0;
        bool found = false;
        for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
            if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
            
            if (currentMonitor == monitorIndex) {
                DEVMODE dm;
                dm.dmSize = sizeof(DEVMODE);
                if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                    x1 = dm.dmPosition.x;
                    y1 = dm.dmPosition.y;
                    width = dm.dmPelsWidth;
                    height = dm.dmPelsHeight;
                    found = true;
                    break;
                }
            }
            currentMonitor++;
        }
        
        if (!found) {
            // Fallback to all monitors
            x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
            y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
            width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }
    }
    
    std::cout << "[Screen] Streaming monitor " << monitorIndex << " (" << width << "x" << height << ")" << std::endl;
    
    // Prepare GDI+ for screen capture
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Adaptive quality and FPS variables
    int frameCount = 0;
    ULONG currentQuality = 100; // Start with 100% quality
    int targetDelay = 33; // Target delay in ms (start at ~30 FPS)
    double actualFPS = 0.0; // FPS thực tế được tính từ thời gian giữa các frame
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    
    while (g_IsStreamingScreen) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        
        // Capture screen to memory DC
        BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x1, y1, SRCCOPY);
        
        // Create full-res bitmap from HBITMAP
        Bitmap fullBitmap(hBitmap, NULL);
        
        // DOWNSCALE giữ nguyên tỉ lệ màn hình gốc
        // Tính tỉ lệ aspect ratio gốc
        float aspectRatio = (float)width / (float)height;
        
        // Quyết định chiều nào làm base để scale (max 1920px cho chiều rộng hoặc 1080px cho chiều cao)
        int targetWidth, targetHeight;
        
        if (width > height) {
            // Màn hình landscape - giới hạn width ở 1920
            if (width > 1920) {
                targetWidth = 1920;
                targetHeight = (int)(targetWidth / aspectRatio);
            } else {
                // Màn hình nhỏ hơn 1920 - giữ nguyên
                targetWidth = width;
                targetHeight = height;
            }
        } else {
            // Màn hình portrait - giới hạn height ở 1080
            if (height > 1080) {
                targetHeight = 1080;
                targetWidth = (int)(targetHeight * aspectRatio);
            } else {
                // Màn hình nhỏ hơn 1080 - giữ nguyên
                targetWidth = width;
                targetHeight = height;
            }
        }
        
        std::cout << "[Screen] Original: " << width << "x" << height 
                  << ", Streaming: " << targetWidth << "x" << targetHeight 
                  << " (ratio: " << aspectRatio << ")" << std::endl;
        
        // Create downscaled bitmap
        Bitmap scaledBitmap(targetWidth, targetHeight, PixelFormat24bppRGB);
        Graphics graphics(&scaledBitmap);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic); // High quality downscale
        
        graphics.DrawImage(&fullBitmap, 0, 0, targetWidth, targetHeight);
        
        // Convert downscaled bitmap to JPEG
        IStream* pStream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        
        CLSID clsid;
        GetEncoderClsid(L"image/jpeg", &clsid);
        
        // Adaptive quality - giảm quality khi encode lâu (mạng yếu/CPU chậm)
        auto encodeStartTime = std::chrono::high_resolution_clock::now();
        
        EncoderParameters encoderParams;
        encoderParams.Count = 1;
        encoderParams.Parameter[0].Guid = EncoderQuality;
        encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParams.Parameter[0].NumberOfValues = 1;
        encoderParams.Parameter[0].Value = &currentQuality;
        
        scaledBitmap.Save(pStream, &clsid, &encoderParams);
        
        auto encodeEndTime = std::chrono::high_resolution_clock::now();
        auto encodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(encodeEndTime - encodeStartTime).count();
        
        // Adjust quality và delay dựa trên encode time (adaptive cho mạng yếu/mạnh)
        if (encodeDuration > 80) {
            // Quá chậm (mạng yếu/CPU chậm) → giảm quality và tăng delay (giảm FPS)
            if (currentQuality > 40) currentQuality -= 10;
            if (targetDelay < 200) targetDelay += 10; // Tăng delay = giảm FPS
            std::cout << "[Screen] Slow (" << encodeDuration << "ms), reducing quality to " << currentQuality << "%, delay to " << targetDelay << "ms" << std::endl;
        } else if (encodeDuration < 40) {
            // Nhanh (mạng mạnh) → tăng quality và giảm delay (tăng FPS)
            if (currentQuality < 100) {
                currentQuality += 10;
                if (currentQuality > 100) currentQuality = 100;
            }
            // Giảm delay để tăng FPS, tối thiểu 1ms
            if (targetDelay > 1) targetDelay -= 2;
        }
        
        LARGE_INTEGER liZero = {};
        pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
        STATSTG stg;
        pStream->Stat(&stg, STATFLAG_NONAME);
        
        DWORD size = (DWORD)stg.cbSize.QuadPart;
        std::vector<unsigned char> buffer(size);
        ULONG bytesRead = 0;
        pStream->Read(&buffer[0], size, &bytesRead);
        pStream->Release();
        
        // Encode to base64 and send
        std::string jpg_b64 = Base64Encode(&buffer[0], size);
        
        // Tính FPS thực tế từ thời gian giữa các frame
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTime).count();
        if (timeSinceLastFrame > 0) {
            actualFPS = 1000.0 / timeSinceLastFrame; // FPS thực
        }
        lastFrameTime = currentTime;
        
        try {
            json j;
            j["type"] = "SCREEN_FRAME";
            j["data"] = jpg_b64;
            j["monitorIndex"] = monitorIndex;
            j["quality"] = currentQuality;
            j["fps"] = (int)actualFPS; // Gửi FPS thực tế, không phải target
            j["size"] = size;
            s->send(hdl, j.dump(), websocketpp::frame::opcode::text);
            
            frameCount++;
            if (frameCount % 100 == 0) {
                std::cout << "[Screen] Sent " << frameCount << " frames, quality: " << currentQuality << "%, actual FPS: " << (int)actualFPS << ", delay: " << targetDelay << "ms, size: " << (size/1024) << "KB" << std::endl;
            }
        }
        catch (...) {
            g_IsStreamingScreen = false;
            break;
        }
        
        // Dynamic delay control
        std::this_thread::sleep_for(std::chrono::milliseconds(targetDelay));
    }
    
    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    std::cout << "[Screen] Streaming stopped" << std::endl;
}

void StartScreenStream(server* s, websocketpp::connection_hdl hdl, int monitorIndex) {
    if (!g_IsStreamingScreen) {
        g_IsStreamingScreen = true;
        g_ScreenThread = new std::thread(ScreenStreamThreadFunc, s, hdl, monitorIndex);
        g_ScreenThread->detach();
    }
}

void StopScreenStream() {
    g_IsStreamingScreen = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}