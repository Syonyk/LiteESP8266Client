# Lite ESP8266 Client
### By Syonyk

# What Is it?
This is a lightweight SoftwareSerial based client for the ESP8266 module - it uses about 100 bytes of SRAM (almost entirely in SoftwareSerial), instead of the 400+ bytes other libraries use.  Why does this matter?  If you want to do something substantial with your wireless interface, you may need a bit more RAM than you're left with after you've put Serial and SoftwareSerial and one of the existing ESP8266 libraries in your code.

To the best of my knowledge and testing, this library is *the* lightest weight ESP8266 AT command set library out there in terms of SRAM use!  Pair it with my LiteSerialLogger code, and you can have a full ESP8266 client, with serial logging, for only 100 bytes of SRAM over the base 9 bytes (millis counters and such).  This is one of my examples, with LiteSerialLogger support enabled, making a series of GET and POST requests, including receiving full 1460 byte packets.  Beat this!

`Global variables use 109 bytes (5%) of dynamic memory, leaving 1,939 bytes for local variables. Maximum is 2,048 bytes.`

# How to Use
Import the library.  You can download a zip file of it above and import it in the Arduino IDE with Sketch -> Include Library -> Add Zip Library.

Look at the examples - they should be in File -> Examples -> LiteESP8266Client -> LiteESP8266Client

You could do something like this (set the proper variables up at the top, and you probably want error handling as well):
```
const char ssid[] PROGMEM = "MySSID;
const char password[] PROGMEM = "MyWirelessPassword";
LiteESP8266 radio;

radio.begin(9600);
radio.set_station_mode();
radio.connect_to_ap(ssid, password);
radio.connect_progmem(host, 80);
radio.send_progmem(http_get_request);
radio.send_progmem(http_useragent);
radio.send_progmem(http_close_connection);

char *data = radio.get_http_response(512);
// Do something with data here.

// Data is malloc'd - so free it.
if (data) free(data);
```

Most of the data is assumed to be in program memory - see how I allocated the constant strings in the example above.  Look at the arguments - most of them are suffixed with `_progmem` and will not work properly with a pointer to data memory.  If you find you need something working with data memory that isn't, and it's not for a silly reason, file a bug and I'll see what I can do.

What's also important to mention is what it *does not support*.  It does not support multiple connections with the MUX feature.  It does not support AP mode, or AP+Station mode.  It simply implements a lightweight client for connecting to an AP, performing basic radio functions, and returning data.

It is really, really important for you to note that returned data has been allocated with malloc - so it is *your* responsibility to free it when you're done with it.  However, the data buffer allocated is only enough for the actual data returned, and you can put a cap on the maximum amount of data to be returned.  This should let you work within your memory requirements (though having more free SRAM makes it a lot easier).




# How Does it Work?
I got greatly annoyed at the state of Arduino libraries for working with an ESP8266 as an AT serial device when I was working on a project.  The libraries just waste SRAM horribly.  Either they have their own buffer (on top of the Serial or SoftwareSerial buffer), they put all their string constants in data memory (Harvard architecture, folks!), or both.  It's easy to end up with 700-800 bytes of SRAM used just to connect to an AP and transfer a bit of data.  *This is a problem* when you only have 2039 usable bytes to start with!

So I wrote my own, based on the principles of minimum SRAM use.  

I only support SoftwareSerial, as that's the most useful for me.  Porting this to work with hardware serial ports wouldn't be tricky, and would actually save a good bit of SRAM (as most of my SRAM use is from the SoftwareSerial read buffers).  All of my string constants are stored in program memory, and the code strongly encourages people to put their string constants in program memory too.

The lazy way to pattern match against a few strings (for searching for the end of a command, or the success/fail result) is to copy the SoftwareSerial buffer into your own buffer, and use `strstr` or such - lazy, and a waste of perfectly good memory.  The more complex way is to implement a string matching state machine that pulls a character at a time off and doesn't require holding the entire string in memory at once.  If you're looking for a success or failure code, you implement two and run them in parallel.  It's really not that hard, but it saves you a ton of memory.

I hate to say it's really not that hard, but I haven't seen any other library that does this yet, and the other ESP8266 AT libraries I've seen are all badly flawed in terms of their SRAM use.

# I Found a Bug!
Great!  File a bug report.  I use this library and will be updating it as I find new things I need - so I'll try to fix bugs with it as well, as I find them or as they're reported.

# This is awesome, can I buy you something?
Eh.  I'm pretty good on stuff.  If you want to buy a bizarre little Chinese electronic gizmo to tear apart, I wouldn't mind.

What I would prefer is for you to learn how to properly write libraries for a Harvard architecture system and not be silly with memory.

