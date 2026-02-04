#pragma once

#include "ofMain.h"

#include <mutex>
#include <string>

#ifdef TARGET_LINUX
#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/raw-utils.h>
#endif

class ofxPipeWire {
public:
    struct VideoConfig {
        int width = 640;
        int height = 480;
        int fps = 30;
    };

    ofxPipeWire();
    ~ofxPipeWire();

    void setAppName(const std::string& name);
    void setNodeName(const std::string& name);

    bool setup(bool enablePublish, bool enableCapture, const VideoConfig& config = VideoConfig());
    void update();
    void shutdown();

    bool isInitialized() const;

    // Publish path (output stream)
    bool submitFrame(const ofPixels& pixels);

    // Capture path (input stream)
    bool getLatestFrame(ofPixels& outPixels);

private:
#ifdef TARGET_LINUX
    struct NegotiatedVideo {
        int width = 0;
        int height = 0;
        int fps = 0;
        spa_video_format format = SPA_VIDEO_FORMAT_UNKNOWN;
        uint32_t stride = 0;
        bool valid = false;
    };

    struct StreamListenerData {
        ofxPipeWire* self = nullptr;
        bool isPublish = false;
    };

    bool setupPipeWire();
    void teardownPipeWire();

    bool createPublishStream();
    bool createCaptureStream();

    spa_pod* buildVideoFormat(spa_pod_builder& builder);
    NegotiatedVideo getDefaultVideoInfo() const;

    static void onRegistryGlobal(void* data, uint32_t id, uint32_t permissions,
                                 const char* type, uint32_t version, const spa_dict* props);

    static void onStreamStateChanged(void* data, enum pw_stream_state oldState,
                                     enum pw_stream_state state, const char* error);

    static void onStreamParamChanged(void* data, uint32_t id, const spa_pod* param);

    static void onPublishProcess(void* data);
    static void onCaptureProcess(void* data);

    void handleCaptureBuffer(pw_buffer* buffer);
    void fillPublishBuffer(pw_buffer* buffer);

    void onVideoFormatChanged(bool isPublish, const spa_video_info_raw& info);

    void ensurePublishFormat(const ofPixels& pixels, const NegotiatedVideo& info);
    void convertToFormat(const ofPixels& src, uint8_t* dst, int dstStride, const NegotiatedVideo& info);
    void convertFromFormat(const uint8_t* src, int srcStride, ofPixels& dst, const NegotiatedVideo& info);

    pw_main_loop* mainLoop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;

    pw_stream* publishStream = nullptr;
    pw_stream* captureStream = nullptr;

    spa_hook registryListener;
    spa_hook publishListener;
    spa_hook captureListener;

    StreamListenerData publishListenerData;
    StreamListenerData captureListenerData;

    VideoConfig videoConfig;
    bool publishEnabled = false;
    bool captureEnabled = false;

    NegotiatedVideo publishInfo;
    NegotiatedVideo captureInfo;

    ofPixels publishFrame;
    bool hasPublishFrame = false;
    std::mutex publishMutex;

    ofPixels latestFrame;
    bool hasLatestFrame = false;
    std::mutex captureMutex;

    std::string appName = "ofxPipeWire";
    std::string nodeName = "ofxPipeWire";
#endif

    bool initialized = false;
};
