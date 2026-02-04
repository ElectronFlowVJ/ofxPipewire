#pragma once

#include "ofMain.h"

#include <mutex>
#include <string>

#ifdef TARGET_LINUX
#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
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
    bool setupPipeWire();
    void teardownPipeWire();

    bool createPublishStream();
    bool createCaptureStream();

    spa_pod* buildVideoFormat(spa_pod_builder& builder);

    static void onRegistryGlobal(void* data, uint32_t id, uint32_t permissions,
                                 const char* type, uint32_t version, const spa_dict* props);

    static void onStreamStateChanged(void* data, enum pw_stream_state oldState,
                                     enum pw_stream_state state, const char* error);

    static void onPublishProcess(void* data);
    static void onCaptureProcess(void* data);

    void handleCaptureBuffer(pw_buffer* buffer);
    void fillPublishBuffer(pw_buffer* buffer);

    void ensurePublishFormat(const ofPixels& pixels);
    void convertToRGBx(const ofPixels& src, ofPixels& dst);

    pw_main_loop* mainLoop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;

    pw_stream* publishStream = nullptr;
    pw_stream* captureStream = nullptr;

    spa_hook registryListener;
    spa_hook publishListener;
    spa_hook captureListener;

    VideoConfig videoConfig;
    bool publishEnabled = false;
    bool captureEnabled = false;

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
