#include "ofApp.h"

void ofApp::setup(){
    ofSetVerticalSync(true);
    ofSetFrameRate(60);

    config.width = 640;
    config.height = 360;
    config.fps = 30;

    pipewire.setAppName("ofxPipeWire capture example");
    pipewire.setNodeName("ofxPipeWire Capture");

    pipewireReady = pipewire.setup(false, true, config);
    if(!pipewireReady){
        status = "PipeWire setup failed (Linux only)";
        ofLogWarning("example-capture") << status;
    }
}

void ofApp::update(){
    pipewire.update();

    if(pipewireReady && !targetSelected){
        pickFirstVideoNode();
    }

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

    if(captureTex.isAllocated()){
        captureTex.draw(20, 20, config.width, config.height);
    }else{
        ofDrawBitmapStringHighlight("Waiting for capture stream...", 20, 40);
    }

    if(!status.empty()){
        ofDrawBitmapStringHighlight(status, 20, 20 + config.height + 30);
    }

    if(!targetName.empty()){
        ofDrawBitmapStringHighlight("Target: " + targetName, 20, 20 + config.height + 55);
    }
}

void ofApp::exit(){
    pipewire.shutdown();
}

void ofApp::pickFirstVideoNode(){
    auto nodes = pipewire.getVideoNodes();
    if(nodes.empty()){
        status = "No video nodes found yet";
        return;
    }

    targetName = nodes.front().name.empty() ? "Unknown" : nodes.front().name;
    pipewire.setCaptureTargetNodeName(targetName);

    pipewire.shutdown();
    pipewireReady = pipewire.setup(false, true, config);

    if(pipewireReady){
        status = "Targeting: " + targetName;
    }else{
        status = "Reconnection failed";
    }

    targetSelected = true;
}
