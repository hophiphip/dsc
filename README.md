# dsc (Dumb Screen Capture)
Takes a screenshot and saves it as [.ppm](https://en.wikipedia.org/wiki/Netpbm) image

## Dependencies
  * [Mongoose Networking Library](https://github.com/cesanta/mongoose)

## Usage
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
```bash
./dsc image_name.ppm
```
