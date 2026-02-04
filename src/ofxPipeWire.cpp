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

    publishInfo = getDefaultVideoInfo();
    captureInfo = getDefaultVideoInfo();

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
    ensurePublishFormat(pixels, publishInfo.valid ? publishInfo : getDefaultVideoInfo());
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
            SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4,
                SPA_VIDEO_FORMAT_RGBx,
                SPA_VIDEO_FORMAT_RGBA,
                SPA_VIDEO_FORMAT_BGRx,
                SPA_VIDEO_FORMAT_BGRA),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(
                SPA_RECTANGLE(videoConfig.width, videoConfig.height),
                SPA_RECTANGLE(16, 16),
                SPA_RECTANGLE(8192, 8192)),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                SPA_FRACTION(videoConfig.fps, 1),
                SPA_FRACTION(1, 1),
                SPA_FRACTION(240, 1))
        )
    );
}

ofxPipeWire::NegotiatedVideo ofxPipeWire::getDefaultVideoInfo() const{
    NegotiatedVideo info;
    info.width = videoConfig.width;
    info.height = videoConfig.height;
    info.fps = videoConfig.fps;
    info.format = SPA_VIDEO_FORMAT_RGBx;
    info.stride = static_cast<uint32_t>(videoConfig.width * 4);
    info.valid = true;
    return info;
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

    publishListenerData.self = this;
    publishListenerData.isPublish = true;

    static const pw_stream_events publishEvents = {
        PW_VERSION_STREAM_EVENTS,
        .state_changed = ofxPipeWire::onStreamStateChanged,
        .param_changed = ofxPipeWire::onStreamParamChanged,
        .process = ofxPipeWire::onPublishProcess
    };

    pw_stream_add_listener(publishStream, &publishListener, &publishEvents, &publishListenerData);

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

    captureListenerData.self = this;
    captureListenerData.isPublish = false;

    static const pw_stream_events captureEvents = {
        PW_VERSION_STREAM_EVENTS,
        .state_changed = ofxPipeWire::onStreamStateChanged,
        .param_changed = ofxPipeWire::onStreamParamChanged,
        .process = ofxPipeWire::onCaptureProcess
    };

    pw_stream_add_listener(captureStream, &captureListener, &captureEvents, &captureListenerData);

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
    StreamListenerData* listenerData = static_cast<StreamListenerData*>(data);
    ofxPipeWire* self = listenerData ? listenerData->self : nullptr;
    if(!self){
        return;
    }

    if(state == PW_STREAM_STATE_ERROR){
        ofLogError("ofxPipeWire") << "Stream error: " << (error ? error : "unknown");
    }
    if(state == PW_STREAM_STATE_STREAMING){
        ofLogNotice("ofxPipeWire") << "Stream is streaming";
    }
}

void ofxPipeWire::onStreamParamChanged(void* data, uint32_t id, const spa_pod* param){
    if(id != SPA_PARAM_Format || param == nullptr){
        return;
    }

    StreamListenerData* listenerData = static_cast<StreamListenerData*>(data);
    ofxPipeWire* self = listenerData ? listenerData->self : nullptr;
    if(!self){
        return;
    }

    spa_video_info_raw info = {};
    if(spa_format_video_raw_parse(param, &info) < 0){
        return;
    }

    self->onVideoFormatChanged(listenerData->isPublish, info);
}

void ofxPipeWire::onPublishProcess(void* data){
    StreamListenerData* listenerData = static_cast<StreamListenerData*>(data);
    ofxPipeWire* self = listenerData ? listenerData->self : nullptr;
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
    StreamListenerData* listenerData = static_cast<StreamListenerData*>(data);
    ofxPipeWire* self = listenerData ? listenerData->self : nullptr;
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

    NegotiatedVideo info = captureInfo.valid ? captureInfo : getDefaultVideoInfo();
    const uint8_t* src = static_cast<const uint8_t*>(data->data) + data->chunk->offset;
    uint32_t stride = data->chunk->stride;
    if(stride == 0){
        stride = info.stride > 0 ? info.stride : static_cast<uint32_t>(info.width * 4);
    }

    std::lock_guard<std::mutex> lock(captureMutex);
    latestFrame.allocate(info.width, info.height, OF_PIXELS_RGBA);
    convertFromFormat(src, static_cast<int>(stride), latestFrame, info);
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

    NegotiatedVideo info = publishInfo.valid ? publishInfo : getDefaultVideoInfo();
    uint8_t* dst = static_cast<uint8_t*>(data->data) + data->chunk->offset;
    uint32_t stride = data->chunk->stride;
    if(stride == 0){
        stride = info.stride > 0 ? info.stride : static_cast<uint32_t>(info.width * 4);
    }

    std::lock_guard<std::mutex> lock(publishMutex);
    if(hasPublishFrame && publishFrame.isAllocated()){
        convertToFormat(publishFrame, dst, static_cast<int>(stride), info);
    }else{
        for(int y = 0; y < info.height; ++y){
            memset(dst + y * stride, 0, info.width * 4);
        }
    }

    data->chunk->offset = 0;
    data->chunk->size = stride * info.height;
    data->chunk->stride = stride;
}

void ofxPipeWire::onVideoFormatChanged(bool isPublish, const spa_video_info_raw& info){
    NegotiatedVideo negotiated;
    negotiated.width = static_cast<int>(info.size.width);
    negotiated.height = static_cast<int>(info.size.height);
    negotiated.format = info.format;
    negotiated.fps = 0;
    if(info.framerate.denom > 0){
        negotiated.fps = static_cast<int>(info.framerate.num / info.framerate.denom);
    }
    negotiated.stride = info.stride[0];
    if(negotiated.stride == 0){
        negotiated.stride = static_cast<uint32_t>(negotiated.width * 4);
    }
    negotiated.valid = true;

    if(isPublish){
        publishInfo = negotiated;
        ofLogNotice("ofxPipeWire") << "Publish format: " << negotiated.width << "x" << negotiated.height;
    }else{
        captureInfo = negotiated;
        ofLogNotice("ofxPipeWire") << "Capture format: " << negotiated.width << "x" << negotiated.height;
    }
}

void ofxPipeWire::ensurePublishFormat(const ofPixels& pixels, const NegotiatedVideo& info){
    if(publishFrame.isAllocated() && publishFrame.getWidth() == info.width && publishFrame.getHeight() == info.height){
        return;
    }

    if(pixels.getWidth() != info.width || pixels.getHeight() != info.height){
        ofLogNotice("ofxPipeWire") << "Resizing input pixels to negotiated size";
    }

    publishFrame.allocate(info.width, info.height, OF_PIXELS_RGBA);
}

void ofxPipeWire::convertToFormat(const ofPixels& src, uint8_t* dst, int dstStride, const NegotiatedVideo& info){
    if(!dst || !src.isAllocated()){
        return;
    }

    ofPixels scaled = src;
    if(src.getWidth() != info.width || src.getHeight() != info.height){
        scaled.resize(info.width, info.height);
    }

    const uint8_t* srcData = scaled.getData();
    int w = info.width;
    int h = info.height;

    if(info.format == SPA_VIDEO_FORMAT_RGBA){
        for(int y = 0; y < h; ++y){
            memcpy(dst + y * dstStride, srcData + y * w * 4, w * 4);
        }
        return;
    }

    for(int y = 0; y < h; ++y){
        const uint8_t* srcRow = srcData + y * w * 4;
        uint8_t* dstRow = dst + y * dstStride;
        for(int x = 0; x < w; ++x){
            const uint8_t r = srcRow[x * 4 + 0];
            const uint8_t g = srcRow[x * 4 + 1];
            const uint8_t b = srcRow[x * 4 + 2];
            const uint8_t a = srcRow[x * 4 + 3];

            switch(info.format){
                case SPA_VIDEO_FORMAT_RGBx:
                    dstRow[x * 4 + 0] = r;
                    dstRow[x * 4 + 1] = g;
                    dstRow[x * 4 + 2] = b;
                    dstRow[x * 4 + 3] = 255;
                    break;
                case SPA_VIDEO_FORMAT_BGRx:
                    dstRow[x * 4 + 0] = b;
                    dstRow[x * 4 + 1] = g;
                    dstRow[x * 4 + 2] = r;
                    dstRow[x * 4 + 3] = 255;
                    break;
                case SPA_VIDEO_FORMAT_BGRA:
                    dstRow[x * 4 + 0] = b;
                    dstRow[x * 4 + 1] = g;
                    dstRow[x * 4 + 2] = r;
                    dstRow[x * 4 + 3] = a;
                    break;
                default:
                    dstRow[x * 4 + 0] = r;
                    dstRow[x * 4 + 1] = g;
                    dstRow[x * 4 + 2] = b;
                    dstRow[x * 4 + 3] = a;
                    break;
            }
        }
    }
}

void ofxPipeWire::convertFromFormat(const uint8_t* src, int srcStride, ofPixels& dst, const NegotiatedVideo& info){
    if(!src || !dst.isAllocated()){
        return;
    }

    int w = info.width;
    int h = info.height;
    uint8_t* dstData = dst.getData();

    if(info.format == SPA_VIDEO_FORMAT_RGBA){
        for(int y = 0; y < h; ++y){
            memcpy(dstData + y * w * 4, src + y * srcStride, w * 4);
        }
        return;
    }

    for(int y = 0; y < h; ++y){
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dstData + y * w * 4;
        for(int x = 0; x < w; ++x){
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            uint8_t a = 255;

            switch(info.format){
                case SPA_VIDEO_FORMAT_RGBx:
                    r = srcRow[x * 4 + 0];
                    g = srcRow[x * 4 + 1];
                    b = srcRow[x * 4 + 2];
                    a = 255;
                    break;
                case SPA_VIDEO_FORMAT_BGRx:
                    b = srcRow[x * 4 + 0];
                    g = srcRow[x * 4 + 1];
                    r = srcRow[x * 4 + 2];
                    a = 255;
                    break;
                case SPA_VIDEO_FORMAT_BGRA:
                    b = srcRow[x * 4 + 0];
                    g = srcRow[x * 4 + 1];
                    r = srcRow[x * 4 + 2];
                    a = srcRow[x * 4 + 3];
                    break;
                default:
                    r = srcRow[x * 4 + 0];
                    g = srcRow[x * 4 + 1];
                    b = srcRow[x * 4 + 2];
                    a = srcRow[x * 4 + 3];
                    break;
            }

            dstRow[x * 4 + 0] = r;
            dstRow[x * 4 + 1] = g;
            dstRow[x * 4 + 2] = b;
            dstRow[x * 4 + 3] = a;
        }
    }
}

#endif
