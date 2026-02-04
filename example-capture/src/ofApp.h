#pragma once

#include "ofMain.h"
#include "ofxPipeWire.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void exit();

private:
    void pickFirstVideoNode();

    ofxPipeWire pipewire;
    ofxPipeWire::VideoConfig config;

    ofPixels capturedPixels;
    ofTexture captureTex;

    std::string targetName;
    std::string status;
    bool pipewireReady = false;
    bool targetSelected = false;
};
