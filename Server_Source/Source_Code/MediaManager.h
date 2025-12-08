#pragma once
#include "Global.h"

std::string CaptureScreenBase64();
void StartWebcamStream(server* s, websocketpp::connection_hdl hdl);
void StartRecordVideo(server* s, websocketpp::connection_hdl hdl);