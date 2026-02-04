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
    void generatePublishFrame();

    ofxPipeWire pipewire;
    ofxPipeWire::VideoConfig config;

    ofPixels publishPixels;
    ofPixels capturedPixels;

    ofTexture publishTex;
    ofTexture captureTex;

    float timeStart = 0.0f;
    bool pipewireReady = false;
};
