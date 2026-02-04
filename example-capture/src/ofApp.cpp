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
        refreshTargets();
        int preferredIndex = choosePreferredIndex();
        if(preferredIndex >= 0){
            connectToTarget(preferredIndex);
        }else{
            status = "No video nodes found yet";
        }
    }

    if(pendingReconnect){
        pendingReconnect = false;
        pipewire.shutdown();
        pipewireReady = pipewire.setup(false, true, config);
        if(pipewireReady){
            status = "Targeting: " + targetName;
        }else{
            status = "Reconnection failed";
        }
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

    ofDrawBitmapStringHighlight("Keys: n=next, p=prev, r=refresh", 20, 20 + config.height + 80);
}

void ofApp::exit(){
    pipewire.shutdown();
}

void ofApp::keyPressed(int key){
    if(!pipewireReady){
        return;
    }

    if(key == 'r'){
        refreshTargets();
        status = "Refreshed targets";
        return;
    }

    if(targets.empty()){
        refreshTargets();
    }

    if(targets.empty()){
        status = "No targets available";
        return;
    }

    if(key == 'n'){
        int nextIndex = targetIndex + 1;
        if(nextIndex >= static_cast<int>(targets.size())){
            nextIndex = 0;
        }
        connectToTarget(nextIndex);
    }else if(key == 'p'){
        int prevIndex = targetIndex - 1;
        if(prevIndex < 0){
            prevIndex = static_cast<int>(targets.size()) - 1;
        }
        connectToTarget(prevIndex);
    }
}

void ofApp::refreshTargets(){
    targets = pipewire.getVideoNodes();
}

int ofApp::choosePreferredIndex() const{
    if(targets.empty()){
        return -1;
    }

    for(size_t i = 0; i < targets.size(); ++i){
        const auto& node = targets[i];
        if(node.mediaClass.find("Video/Source") != std::string::npos){
            return static_cast<int>(i);
        }
    }

    for(size_t i = 0; i < targets.size(); ++i){
        const auto& node = targets[i];
        if(node.mediaClass.find("Video") != std::string::npos){
            return static_cast<int>(i);
        }
    }

    return 0;
}

void ofApp::connectToTarget(int index){
    if(index < 0 || index >= static_cast<int>(targets.size())){
        return;
    }

    targetIndex = index;
    targetName = targets[index].name.empty() ? "Unknown" : targets[index].name;
    pipewire.setCaptureTargetNodeName(targetName);
    pendingReconnect = true;
    targetSelected = true;
}
