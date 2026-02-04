#include "ofApp.h"

#include <cmath>
#include <sstream>

void ofApp::setup(){
    ofSetVerticalSync(true);
    ofSetFrameRate(60);

    config.width = 640;
    config.height = 360;
    config.fps = 30;

    pipewire.setAppName("ofxPipeWire example");
    pipewire.setNodeName("ofxPipeWire Video");

    pipewireReady = pipewire.setup(true, true, config);
    if(!pipewireReady){
        ofLogWarning("example-basic") << "PipeWire setup failed. This example is Linux-only.";
    }

    publishPixels.allocate(config.width, config.height, OF_PIXELS_RGBA);
    publishTex.allocate(config.width, config.height, GL_RGBA);

    captureTex.allocate(config.width, config.height, GL_RGBA);

    timeStart = ofGetElapsedTimef();
}

void ofApp::update(){
    pipewire.update();

    generatePublishFrame();
    pipewire.submitFrame(publishPixels);

    publishTex.loadData(publishPixels);

    if(pipewire.getLatestFrame(capturedPixels)){
        if(!captureTex.isAllocated()){
            captureTex.allocate(capturedPixels.getWidth(), capturedPixels.getHeight(), GL_RGBA);
        }
        captureTex.loadData(capturedPixels);
    }
}

void ofApp::draw(){
    ofBackground(12);
    ofSetColor(255);

    int w = config.width;
    int h = config.height;

    if(publishTex.isAllocated()){
        publishTex.draw(20, 20, w, h);
    }

    if(captureTex.isAllocated() && capturedPixels.isAllocated()){
        captureTex.draw(40 + w, 20, w, h);
    }else{
        ofDrawBitmapStringHighlight("Waiting for capture stream...", 40 + w, 40);
    }

    ofDrawBitmapStringHighlight("Publish", 20, 20 + h + 20);
    ofDrawBitmapStringHighlight("Capture", 40 + w, 20 + h + 20);

    float listTop = 20.0f + h + 60.0f;
    ofDrawBitmapStringHighlight("Video Nodes (first " + ofToString(maxListCount) + ")", 20, listTop);

    auto nodes = pipewire.getVideoNodes();
    float y = listTop + 18.0f;
    int count = 0;
    for(const auto& node : nodes){
        ofDrawBitmapString(formatNodeLine(node), 20, y);
        y += 14.0f;
        if(++count >= maxListCount){
            break;
        }
    }

    y += 10.0f;
    ofDrawBitmapStringHighlight("Ports (first " + ofToString(maxListCount) + ")", 20, y);
    y += 18.0f;

    auto ports = pipewire.getPorts();
    count = 0;
    for(const auto& port : ports){
        ofDrawBitmapString(formatPortLine(port), 20, y);
        y += 14.0f;
        if(++count >= maxListCount){
            break;
        }
    }

    if(!pipewireReady){
        ofDrawBitmapStringHighlight("PipeWire not initialized (Linux only)", 20, y + 20);
    }
}

void ofApp::exit(){
    pipewire.shutdown();
}

void ofApp::generatePublishFrame(){
    if(!publishPixels.isAllocated()){
        return;
    }

    int w = publishPixels.getWidth();
    int h = publishPixels.getHeight();

    float t = ofGetElapsedTimef() - timeStart;
    uint8_t* data = publishPixels.getData();

    for(int y = 0; y < h; ++y){
        float v = static_cast<float>(y) / static_cast<float>(h);
        for(int x = 0; x < w; ++x){
            float u = static_cast<float>(x) / static_cast<float>(w);
            int i = (y * w + x) * 4;

            float r = 0.5f + 0.5f * sinf(t + u * 6.2831f);
            float g = 0.5f + 0.5f * sinf(t * 0.7f + v * 6.2831f);
            float b = 0.5f + 0.5f * sinf(t * 1.3f + (u + v) * 3.1415f);

            data[i + 0] = static_cast<uint8_t>(r * 255.0f);
            data[i + 1] = static_cast<uint8_t>(g * 255.0f);
            data[i + 2] = static_cast<uint8_t>(b * 255.0f);
            data[i + 3] = 255;
        }
    }
}

std::string ofApp::formatNodeLine(const ofxPipeWire::NodeInfo& node) const{
    std::ostringstream oss;
    oss << "#" << node.id << " " << node.name;
    if(!node.mediaClass.empty()){
        oss << " (" << node.mediaClass << ")";
    }
    return oss.str();
}

std::string ofApp::formatPortLine(const ofxPipeWire::PortInfo& port) const{
    std::ostringstream oss;
    oss << "#" << port.id << " node " << port.nodeId << " " << port.direction << " " << port.name;
    if(!port.alias.empty()){
        oss << " alias " << port.alias;
    }
    return oss.str();
}
