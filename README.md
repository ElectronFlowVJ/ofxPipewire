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
- Current implementation uses RGBx format at the configured size and fps.
- If you submit RGB or RGBA pixels, they are converted to RGBx.
- If the input size doesn’t match the configured size, it is resized.

## Roadmap
- Audio stream (capture + publish)
- More flexible format negotiation
