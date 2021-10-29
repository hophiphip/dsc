# Test server for dsc
  - Starts listening for incoming websocket connections
  - Writes received bytes to the canvas

## Quick start
Run server
```bash
go run server.go
```
Connect with client
```bash
./dsc -s ws://localhost:8080/ws
```

## TODO
- [ ] Fix incorrect canvas behavior

