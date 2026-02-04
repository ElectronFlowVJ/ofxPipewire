# ofxPipeWire

PipeWire video (and future audio) streaming addon for openFrameworks on Linux.

## Status
- Video streaming core is implemented (publish + capture) using PipeWire `pw_stream`.
- Audio is scaffolded but not implemented yet.

## Requirements
- Linux with PipeWire installed
- `libpipewire-0.3` development package available via `pkg-config`

## Examples
- `example-basic` publishes a generated video stream and displays the captured stream side-by-side.

## Quick start
1. Add the addon to your project.
2. Initialize it and enable both publish + capture.

```cpp
ofxPipeWire pipewire;

void ofApp::setup(){
    ofxPipeWire::VideoConfig cfg;
    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps = 30;

    pipewire.setAppName("ofApp");
    pipewire.setNodeName("ofxPipeWire Stream");
    pipewire.setup(true, true, cfg);
}

void ofApp::update(){
    pipewire.update();

    // Publish a frame
    ofPixels pixels;
    // fill pixels ...
    pipewire.submitFrame(pixels);

    // Capture a frame
    ofPixels captured;
    if(pipewire.getLatestFrame(captured)){
        // use captured pixels
    }
}

void ofApp::exit(){
    pipewire.shutdown();
}
```

## Notes
- Format negotiation now advertises RGBx, BGRx, RGBA, and BGRA.
- The negotiated size and stride are honored for publish and capture.
- If you submit RGB or RGBA pixels, they are converted to the negotiated format.
- If the input size doesn’t match the negotiated size, it is resized.

## Roadmap
- Audio stream (capture + publish)
- More flexible format negotiation
