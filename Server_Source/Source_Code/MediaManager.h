#pragma once
#include "Global.h"

// Screenshot
std::string CaptureScreenBase64();

// Camera functions
std::string ScanAvailableCameras();
void StartWebcamStream(server* s, websocketpp::connection_hdl hdl, int cameraIndex = 0);
void StartRecordVideo(server* s, websocketpp::connection_hdl hdl, int duration = 10, int cameraIndex = 0);

// Microphone functions  
std::string ScanAvailableMicrophones();
void StartMicStream(server* s, websocketpp::connection_hdl hdl, int micIndex = 0);
void StopMicStream();
void RecordMicAudio(server* s, websocketpp::connection_hdl hdl, int duration = 10, int micIndex = 0);