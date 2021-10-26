package main

import (
	"flag"
	"html/template"
	"log"
	"net/http"

	"github.com/gorilla/websocket"
)

var addr = flag.String("addr", "localhost:8080", "http service address")

var upgraderWs = websocket.Upgrader{}   // use default options
var upgraderEcho = websocket.Upgrader{} // use default options

var message_buf []byte

func ws(w http.ResponseWriter, r *http.Request) {
	c, err := upgraderWs.Upgrade(w, r, nil)
	if err != nil {
		log.Print("upgrade:", err)
		return
	}
	defer c.Close()
	for {
		_, message, err := c.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway) {
				log.Println("client closed unexpectedly")
			} else {
				log.Println("read:", err)
			}
			break
		}

		if len(message_buf) < len(message) {
			message_buf = make([]byte, len(message))
		}

		copy(message_buf, message)

		log.Printf("recieved: %d bytes", len(message_buf))
	}
}

func echo(w http.ResponseWriter, r *http.Request) {
	c, err := upgraderEcho.Upgrade(w, r, nil)
	if err != nil {
		log.Print("upgrade:", err)
		return
	}
	defer c.Close()
	for {
		_, message, err := c.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway) {
				log.Println("client closed unexpectedly")
			} else {
				log.Println("read:", err)
			}
			break
		}

		log.Println("/echo: received: %s", message)
		log.Println("/echo: send len: %d", len(message_buf))

		BinaryMessage := 2
		err = c.WriteMessage(BinaryMessage, message_buf)
		if err != nil {
			log.Println("write:", err)
			break
		}
	}
}

func home(w http.ResponseWriter, r *http.Request) {
	homeTemplate.Execute(w, "ws://"+r.Host+"/echo")
}

func main() {
	flag.Parse()
	log.SetFlags(0)
	http.HandleFunc("/ws", ws)
	http.HandleFunc("/echo", echo)
	http.HandleFunc("/", home)
	log.Fatal(http.ListenAndServe(*addr, nil))
}

// TODO: Fix incorrect canvas behaviour
// TODO: Use `embed` package to embed .html
var homeTemplate = template.Must(template.New("").Parse(`
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<script>  
window.addEventListener("load", function(evt) {

    var output = document.getElementById("output");
    var input = document.getElementById("input");
    var ws;

    var print = function(message) {
        var d = document.createElement("div");
        d.textContent = message;
        output.appendChild(d);
        output.scroll(0, output.scrollHeight);
    };

    document.getElementById("open").onclick = function(evt) {
        if (ws) {
            return false;
        }
        ws = new WebSocket("{{.}}");
        ws.onopen = function(evt) {
            print("OPEN");
        }
        ws.onclose = function(evt) {
            print("CLOSE");
            ws = null;
        }
        ws.onmessage = function(evt) {
						if (evt.data instanceof Blob) {
								reader = new FileReader();

								reader.onload = () => {
										console.log("Result: ", reader.result.length);
										var canvas = document.getElementById("canvas");
										if (canvas.getContext) {
											var ctx = canvas.getContext('2d');

											for (var y = 0; y < 540; y++) {
													for (var x = 0; x < 960; x++) {
															var i = (y * 960 + x) * 4;
															ctx.fillStyle = 'rgb(' + 
																reader.result[i + 2].charCodeAt(0) + ',' +
																reader.result[i + 1].charCodeAt(0) + ',' +
																reader.result[i    ].charCodeAt(0) + ')';
																
															ctx.fillRect(x, y, 1, 1);
												}
											}
										}
								}

								reader.readAsBinaryString(evt.data);
						} else {
								console.log("Result: ", evt.data);
						}
        }

        ws.onerror = function(evt) {
            print("ERROR: " + evt.data);
        }
        return false;
    };

    document.getElementById("send").onclick = function(evt) {
        if (!ws) {
            return false;
        }
        print("SEND: " + input.value);
        ws.send(input.value);
        return false;
    };

    document.getElementById("close").onclick = function(evt) {
        if (!ws) {
            return false;
        }
        ws.close();
        return false;
    };

});
</script>
</head>
<body>
<table>
<tr><td valign="top" width="50%">
<p>Click "Open" to create a connection to the server, 
"Send" to send a message to the server and "Close" to close the connection. 
You can change the message and send multiple times.
<p>
<form>
<button id="open">Open</button>
<button id="close">Close</button>
<p><input id="input" type="text" value="Hello world!">
<button id="send">Send</button>
</form>
</td><td valign="top" width="50%">
<div id="output" style="max-height: 70vh;overflow-y: scroll;"></div>
</td></tr></table>
<canvas id="canvas" width="1920" height="1080"></canvas>
</body>
</html>
`))
