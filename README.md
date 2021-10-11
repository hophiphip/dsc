# dsc (Dumb Screen Capture)
  - Takes a screenshot and saves it as [.ppm](https://en.wikipedia.org/wiki/Netpbm) image.
  - Streams screen image via websockets.

## Dependencies
  * [Mongoose Networking Library](https://github.com/cesanta/mongoose)

## Quick start
### Add submodules
Initialize local configuration
```bash
git submodule init
```
Fetch submodules
```bash
git submodule update
```

### Build project
```bash
cmake .
cmake --build .
```

### Run
1. Take a screenshot
```bash
./dsc -i image_name.ppm
```
2. Start screen streaming
```bash
./dsc -s ws://streaming_server_hostname:streaming_server_port
```
