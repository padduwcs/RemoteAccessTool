#include "MediaManager.h"

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
void WebcamThreadFunc(server* s, websocketpp::connection_hdl hdl) {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
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
void RecordVideoThread(server* s, websocketpp::connection_hdl hdl, int duration) {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
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

void StartWebcamStream(server* s, websocketpp::connection_hdl hdl) {
    if (!g_IsStreaming) {
        g_IsStreaming = true;
        g_WebcamThread = new std::thread(WebcamThreadFunc, s, hdl);
        g_WebcamThread->detach();
    }
}

void StartRecordVideo(server* s, websocketpp::connection_hdl hdl, int duration) {
    std::thread t(RecordVideoThread, s, hdl, duration);
    t.detach();
}