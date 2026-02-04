#pragma once

#include "ofMain.h"
#include "ofxPipeWire.h"

#include <vector>

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void exit();
    void keyPressed(int key);

private:
    void refreshTargets();
    void connectToTarget(int index);
    int choosePreferredIndex() const;

    ofxPipeWire pipewire;
    ofxPipeWire::VideoConfig config;

    ofPixels capturedPixels;
    ofTexture captureTex;

    std::vector<ofxPipeWire::NodeInfo> targets;
    int targetIndex = -1;

    std::string targetName;
    std::string status;
    bool pipewireReady = false;
    bool targetSelected = false;
    bool pendingReconnect = false;
};
