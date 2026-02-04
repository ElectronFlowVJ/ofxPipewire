#include "ofxPipeWire.h"

#ifdef TARGET_LINUX
#include <spa/utils/defs.h>
#endif

ofxPipeWire::ofxPipeWire() = default;

ofxPipeWire::~ofxPipeWire(){
    shutdown();
}

void ofxPipeWire::setAppName(const std::string& name){
#ifdef TARGET_LINUX
    appName = name;
#else
    (void)name;
#endif
}

void ofxPipeWire::setNodeName(const std::string& name){
#ifdef TARGET_LINUX
    nodeName = name;
#else
    (void)name;
#endif
}

bool ofxPipeWire::setup(bool enablePublish, bool enableCapture, const VideoConfig& config){
#ifdef TARGET_LINUX
    if(initialized){
        return true;
    }

    publishEnabled = enablePublish;
    captureEnabled = enableCapture;
    videoConfig = config;

    if(!publishEnabled && !captureEnabled){
        ofLogWarning("ofxPipeWire") << "setup called with no streams enabled";
        return false;
    }

    if(!setupPipeWire()){
        shutdown();
        return false;
    }

    if(publishEnabled && !createPublishStream()){
        shutdown();
        return false;
    }

    if(captureEnabled && !createCaptureStream()){
        shutdown();
        return false;
    }

    initialized = true;
    return true;
#else
    (void)enablePublish;
    (void)enableCapture;
    (void)config;
    ofLogWarning("ofxPipeWire") << "PipeWire is only supported on Linux";
    return false;
#endif
}

void ofxPipeWire::update(){
#ifdef TARGET_LINUX
    if(!initialized || !mainLoop){
        return;
    }

    pw_main_loop_iterate(mainLoop, 0);
#endif
}

void ofxPipeWire::shutdown(){
#ifdef TARGET_LINUX
    if(!initialized){
        return;
    }

    if(publishStream){
        pw_stream_destroy(publishStream);
        publishStream = nullptr;
    }

    if(captureStream){
        pw_stream_destroy(captureStream);
        captureStream = nullptr;
    }

    teardownPipeWire();

    initialized = false;
#endif
}

bool ofxPipeWire::isInitialized() const{
    return initialized;
}

bool ofxPipeWire::submitFrame(const ofPixels& pixels){
#ifdef TARGET_LINUX
    if(!initialized || !publishEnabled || pixels.isAllocated() == false){
        return false;
    }

    std::lock_guard<std::mutex> lock(publishMutex);
    ensurePublishFormat(pixels);
    convertToRGBx(pixels, publishFrame);
    hasPublishFrame = true;
    return true;
#else
    (void)pixels;
    return false;
#endif
}

bool ofxPipeWire::getLatestFrame(ofPixels& outPixels){
#ifdef TARGET_LINUX
    if(!initialized || !captureEnabled){
        return false;
    }

    std::lock_guard<std::mutex> lock(captureMutex);
    if(!hasLatestFrame){
        return false;
    }

    outPixels = latestFrame;
    return true;
#else
    (void)outPixels;
    return false;
#endif
}

#ifdef TARGET_LINUX

bool ofxPipeWire::setupPipeWire(){
    pw_init(nullptr, nullptr);

    mainLoop = pw_main_loop_new(nullptr);
    if(!mainLoop){
        ofLogError("ofxPipeWire") << "Failed to create PipeWire main loop";
        return false;
    }

    context = pw_context_new(pw_main_loop_get_loop(mainLoop), nullptr, 0);
    if(!context){
        ofLogError("ofxPipeWire") << "Failed to create PipeWire context";
        return false;
    }

    core = pw_context_connect(context, nullptr, 0);
    if(!core){
        ofLogError("ofxPipeWire") << "Failed to connect PipeWire core";
        return false;
    }

    registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if(!registry){
        ofLogError("ofxPipeWire") << "Failed to get PipeWire registry";
        return false;
    }

    static const pw_registry_events registryEvents = {
        PW_VERSION_REGISTRY_EVENTS,
        .global = ofxPipeWire::onRegistryGlobal
    };

    pw_registry_add_listener(registry, &registryListener, &registryEvents, this);
    return true;
}

void ofxPipeWire::teardownPipeWire(){
    if(registry){
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
        registry = nullptr;
    }

    if(core){
        pw_core_disconnect(core);
        core = nullptr;
    }

    if(context){
        pw_context_destroy(context);
        context = nullptr;
    }

    if(mainLoop){
        pw_main_loop_destroy(mainLoop);
        mainLoop = nullptr;
    }

    pw_deinit();
}

spa_pod* ofxPipeWire::buildVideoFormat(spa_pod_builder& builder){
    return reinterpret_cast<spa_pod*>(
        spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_Id(SPA_VIDEO_FORMAT_RGBx),
            SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(videoConfig.width, videoConfig.height),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(videoConfig.fps, 1)
        )
    );
}

bool ofxPipeWire::createPublishStream(){
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        PW_KEY_APP_NAME, appName.c_str(),
        PW_KEY_NODE_NAME, nodeName.c_str(),
        nullptr
    );

    publishStream = pw_stream_new(core, "ofxPipeWire Publish", props);
    if(!publishStream){
        ofLogError("ofxPipeWire") << "Failed to create publish stream";
        return false;
    }

    static const pw_stream_events publishEvents = {
        PW_VERSION_STREAM_EVENTS,
        .state_changed = ofxPipeWire::onStreamStateChanged,
        .process = ofxPipeWire::onPublishProcess
    };

    pw_stream_add_listener(publishStream, &publishListener, &publishEvents, this);

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* params[1];
    params[0] = buildVideoFormat(builder);

    int res = pw_stream_connect(
        publishStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params,
        1
    );

    if(res < 0){
        ofLogError("ofxPipeWire") << "Failed to connect publish stream";
        return false;
    }

    return true;
}

bool ofxPipeWire::createCaptureStream(){
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Screen",
        PW_KEY_APP_NAME, appName.c_str(),
        PW_KEY_NODE_NAME, nodeName.c_str(),
        nullptr
    );

    captureStream = pw_stream_new(core, "ofxPipeWire Capture", props);
    if(!captureStream){
        ofLogError("ofxPipeWire") << "Failed to create capture stream";
        return false;
    }

    static const pw_stream_events captureEvents = {
        PW_VERSION_STREAM_EVENTS,
        .state_changed = ofxPipeWire::onStreamStateChanged,
        .process = ofxPipeWire::onCaptureProcess
    };

    pw_stream_add_listener(captureStream, &captureListener, &captureEvents, this);

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* params[1];
    params[0] = buildVideoFormat(builder);

    int res = pw_stream_connect(
        captureStream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params,
        1
    );

    if(res < 0){
        ofLogError("ofxPipeWire") << "Failed to connect capture stream";
        return false;
    }

    return true;
}

void ofxPipeWire::onRegistryGlobal(void* data, uint32_t id, uint32_t permissions,
                                  const char* type, uint32_t version, const spa_dict* props){
    (void)data;
    (void)id;
    (void)permissions;
    (void)type;
    (void)version;
    (void)props;
}

void ofxPipeWire::onStreamStateChanged(void* data, enum pw_stream_state oldState,
                                      enum pw_stream_state state, const char* error){
    (void)oldState;
    ofxPipeWire* self = static_cast<ofxPipeWire*>(data);
    if(state == PW_STREAM_STATE_ERROR){
        ofLogError("ofxPipeWire") << "Stream error: " << (error ? error : "unknown");
    }
    if(state == PW_STREAM_STATE_STREAMING){
        ofLogNotice("ofxPipeWire") << "Stream is streaming";
    }
}

void ofxPipeWire::onPublishProcess(void* data){
    ofxPipeWire* self = static_cast<ofxPipeWire*>(data);
    if(!self || !self->publishStream){
        return;
    }

    pw_buffer* buffer = pw_stream_dequeue_buffer(self->publishStream);
    if(!buffer){
        return;
    }

    self->fillPublishBuffer(buffer);
    pw_stream_queue_buffer(self->publishStream, buffer);
}

void ofxPipeWire::onCaptureProcess(void* data){
    ofxPipeWire* self = static_cast<ofxPipeWire*>(data);
    if(!self || !self->captureStream){
        return;
    }

    pw_buffer* buffer = pw_stream_dequeue_buffer(self->captureStream);
    if(!buffer){
        return;
    }

    self->handleCaptureBuffer(buffer);
    pw_stream_queue_buffer(self->captureStream, buffer);
}

void ofxPipeWire::handleCaptureBuffer(pw_buffer* buffer){
    if(!buffer || !buffer->buffer || buffer->buffer->datas[0].data == nullptr){
        return;
    }

    spa_buffer* spaBuffer = buffer->buffer;
    spa_data* data = &spaBuffer->datas[0];
    if(!data->chunk){
        return;
    }

    const uint8_t* src = static_cast<const uint8_t*>(data->data) + data->chunk->offset;
    uint32_t stride = data->chunk->stride;
    if(stride == 0){
        stride = static_cast<uint32_t>(videoConfig.width * 4);
    }

    std::lock_guard<std::mutex> lock(captureMutex);
    latestFrame.allocate(videoConfig.width, videoConfig.height, OF_PIXELS_RGBA);

    for(int y = 0; y < videoConfig.height; ++y){
        const uint8_t* srcRow = src + y * stride;
        uint8_t* dstRow = latestFrame.getData() + y * videoConfig.width * 4;
        memcpy(dstRow, srcRow, videoConfig.width * 4);
    }

    hasLatestFrame = true;
}

void ofxPipeWire::fillPublishBuffer(pw_buffer* buffer){
    if(!buffer || !buffer->buffer || buffer->buffer->datas[0].data == nullptr){
        return;
    }

    spa_buffer* spaBuffer = buffer->buffer;
    spa_data* data = &spaBuffer->datas[0];
    if(!data->chunk){
        return;
    }

    uint8_t* dst = static_cast<uint8_t*>(data->data) + data->chunk->offset;
    uint32_t stride = data->chunk->stride;
    if(stride == 0){
        stride = static_cast<uint32_t>(videoConfig.width * 4);
    }

    std::lock_guard<std::mutex> lock(publishMutex);
    if(hasPublishFrame && publishFrame.isAllocated()){
        for(int y = 0; y < videoConfig.height; ++y){
            const uint8_t* srcRow = publishFrame.getData() + y * videoConfig.width * 4;
            uint8_t* dstRow = dst + y * stride;
            memcpy(dstRow, srcRow, videoConfig.width * 4);
        }
    }else{
        for(int y = 0; y < videoConfig.height; ++y){
            memset(dst + y * stride, 0, videoConfig.width * 4);
        }
    }

    data->chunk->offset = 0;
    data->chunk->size = stride * videoConfig.height;
    data->chunk->stride = stride;
}

void ofxPipeWire::ensurePublishFormat(const ofPixels& pixels){
    if(publishFrame.isAllocated() && publishFrame.getWidth() == videoConfig.width && publishFrame.getHeight() == videoConfig.height){
        return;
    }

    if(pixels.getWidth() != videoConfig.width || pixels.getHeight() != videoConfig.height){
        ofLogNotice("ofxPipeWire") << "Resizing input pixels to configured size";
    }

    publishFrame.allocate(videoConfig.width, videoConfig.height, OF_PIXELS_RGBA);
}

void ofxPipeWire::convertToRGBx(const ofPixels& src, ofPixels& dst){
    if(!dst.isAllocated()){
        return;
    }

    if(src.getWidth() != videoConfig.width || src.getHeight() != videoConfig.height){
        ofPixels scaled = src;
        scaled.resize(videoConfig.width, videoConfig.height);
        convertToRGBx(scaled, dst);
        return;
    }

    int channels = src.getNumChannels();
    const uint8_t* srcData = src.getData();
    uint8_t* dstData = dst.getData();

    if(channels == 4){
        memcpy(dstData, srcData, videoConfig.width * videoConfig.height * 4);
        return;
    }

    if(channels == 3){
        int pixels = videoConfig.width * videoConfig.height;
        for(int i = 0; i < pixels; ++i){
            int srcIndex = i * 3;
            int dstIndex = i * 4;
            dstData[dstIndex + 0] = srcData[srcIndex + 0];
            dstData[dstIndex + 1] = srcData[srcIndex + 1];
            dstData[dstIndex + 2] = srcData[srcIndex + 2];
            dstData[dstIndex + 3] = 255;
        }
        return;
    }

    ofLogWarning("ofxPipeWire") << "Unsupported pixel format, expected 3 or 4 channels";
}

#endif
