#pragma once

#include "ofMain.h"

#include <mutex>
#include <string>
#include <vector>

#ifdef TARGET_LINUX
#include <pipewire/pipewire.h>
#include <pipewire/keys.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/raw-utils.h>
#include <spa/utils/dict.h>
#endif

class ofxPipeWire {
public:
    enum class VideoFormatPreference {
        RGBA,
        BGRA,
        RGBx,
        BGRx
    };

    struct VideoConfig {
        int width = 640;
        int height = 480;
        int fps = 30;
    };

    struct NodeInfo {
        uint32_t id = 0;
        std::string name;
        std::string description;
        std::string mediaClass;
        std::string objectSerial;
    };

    struct PortInfo {
        uint32_t id = 0;
        uint32_t nodeId = 0;
        std::string name;
        std::string direction;
        std::string alias;
    };

    ofxPipeWire();
    ~ofxPipeWire();

    void setAppName(const std::string& name);
    void setNodeName(const std::string& name);

    void setPreferredVideoFormats(const std::vector<VideoFormatPreference>& formats);

    void setPublishTargetNodeName(const std::string& nodeName);
    void setPublishTargetObjectSerial(const std::string& objectSerial);
    void setCaptureTargetNodeName(const std::string& nodeName);
    void setCaptureTargetObjectSerial(const std::string& objectSerial);

    std::vector<NodeInfo> getNodes() const;
    std::vector<NodeInfo> getVideoNodes() const;
    std::vector<PortInfo> getPorts() const;

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
    static void onRegistryGlobalRemove(void* data, uint32_t id);

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

    void addNodeInfo(uint32_t id, const spa_dict* props);
    void addPortInfo(uint32_t id, const spa_dict* props);
    void removeObject(uint32_t id);
    bool isVideoNode(const NodeInfo& node) const;
    static uint32_t parseUint32(const char* value);

    static spa_video_format toSpaFormat(VideoFormatPreference format);

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

    std::vector<VideoFormatPreference> preferredFormats;

    NegotiatedVideo publishInfo;
    NegotiatedVideo captureInfo;

    std::vector<NodeInfo> nodes;
    std::vector<PortInfo> ports;
    mutable std::mutex discoveryMutex;

    ofPixels publishFrame;
    bool hasPublishFrame = false;
    std::mutex publishMutex;

    ofPixels latestFrame;
    bool hasLatestFrame = false;
    std::mutex captureMutex;

    std::string appName = "ofxPipeWire";
    std::string nodeName = "ofxPipeWire";

    std::string publishTargetObject;
    std::string captureTargetObject;
#endif

    bool initialized = false;
};
