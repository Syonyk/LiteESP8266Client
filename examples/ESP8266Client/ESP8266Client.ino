#include <LiteESP8266Client.h>

// Uncomment this if you have the LiteSerialLogger library installed.  This will
// save about 200 bytes of SRAM.
//#define USE_LITE_SERIAL

// If LiteSerialLogger is installed, use it - otherwise just use the stock
// Serial library.
#ifdef USE_LITE_SERIAL
#include <LiteSerialLogger.h>
#define LOGGER LiteSerial
#else
#define LOGGER Serial
#endif

// Radio!
LiteESP8266 radio;

// Set your SSID and password here, obviously.
// If you don't have a password, you should be able to leave it blank.
const char ssid[] PROGMEM = "MY_SSID";
const char password[] PROGMEM = "MY_WIFI_PASSWORD";

/**
 *This is a test host I use - see below for the PHP scripts running on it.
 * If you don't have it, you can still see how things work and use the code for
 * your own project, but this sample won't work.
 * 
 * test/get_bytes.php
<?php
if (isset($_GET['bytes'])) {
  $count = $_GET['bytes'];
} else {
  $count = 100;
}

for ($i = 0; $i < $count; $i++) {
  print ($i % 10);
}
?>

 * test/post_echo.php:
<?php
print_r($_POST);
?>
 */
const char host[] PROGMEM = "192.168.0.118";

/**
 * Sending HTTP GET and POST request types should send proper headers.
 * 
 * For a basic GET, you'll send something like this.  Note that the lines are
 * all CRLF terminated, with a double CRLF after the last line of the request.
 * 
 * GET /test/get_bytes.php?bytes=500 HTTP/1.0\r\n
 * User-Agent: SyonykArduino/0.1\r\n
 * Connection: close\r\n\r\n
 * 
 * To make a POST, you want to do something like this.  Note the double CRLF
 * before the actual POST data (which is remarkably simple).
 *
 * POST /test/post_echo.php HTTP/1.0\r\n
 * User-Agent: SyonykArduino/0.1\r\n
 * Content-Type: application/x-www-form-urlencoded\r\n
 * Content-length: 28\r\n
 * Connection: close\r\n\r\n
 * foo=1&bar=2&baz=asdfasdfasdf
 */

// Opening line for a GET of 500 bytes from the test host.
const char http_get_500_bytes_request[] PROGMEM = 
    "GET /test/get_bytes.php?bytes=500 HTTP/1.0\r\n";

// Opening line of a GET request to the default page on a host.
const char http_get_index[] PROGMEM = "GET / HTTP/1.0\r\n";

// HTTP POST header strings.
const char http_post_request[] PROGMEM =
        "POST /test/post_echo.php HTTP/1.0\r\n";
const char http_post_content_type[] PROGMEM = 
        "Content-Type: application/x-www-form-urlencoded\r\n";
const char http_post_content_length[] PROGMEM = "Content-length: ";
const char http_post_crlf[] PROGMEM = "\r\n";

// Actual POST data (this is how a form gets encoded).
const char http_post_content[] PROGMEM = "foo=1&bar=2&baz=asdfasdfasdf";

// Common headers.  Note that the close_connection string has a double CRLF.
const char http_useragent[] PROGMEM = "User-Agent: SyonykArduino/0.1\r\n";
const char http_close_connection[] PROGMEM = "Connection: close\r\n\r\n";

// For testing a get to google.com - store the hostname in program memory.
const char google_domain[] PROGMEM = "www.google.com";


// Get (and print) whatever it is google.com is serving up.
void make_http_get_multiple_packets() {
  char *data;

  LOGGER.println(F("Trying to fetch http://www.google.com/..."));
  
  // Connect (or try to).
  if (radio.connect_progmem(google_domain, 80)) {
    LOGGER.println(F("Connect (to Google) success."));
  } else {
    LOGGER.println(F("Connect (to Google) failure."));
    return;
  }

  // Send the proper headers in order.  Note that http_close_connection already
  // has the double CRLF so has to be sent last.
  radio.send_progmem(http_get_index);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  // The largest response I've seen "in the wild" is 1460 bytes - packet size.
  // Getting 1460 bytes of data at 9600 baud takes about 1.5 seconds.
  // Give it 10s, because it can take a while to push everything through.
  // This will wait 10s after the last packet, as it cannot tell the data stream
  // is done other than by not seeing any more responses.
  while (data = radio.get_response_packet(1500, 10000)) {
    if (data) {
      LOGGER.print(F("Got response packet of size: "));
      LOGGER.println(strlen(data));
      free(data);
    } else {
      // This should not happen.
      LOGGER.println(F("Error: Data came back null."));
    }
  }
}

// Get a blob of data and parse it as a HTTP response.
void make_http_get() {
  char *data;
  LOGGER.println(F("Requesting 500 bytes from get_bytes.php..."));
  
  //  ==== Test 500 bytes ====
  if (radio.connect_progmem(host, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_get_500_bytes_request);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  data = radio.get_http_response(512);
  
  if (data) {
    LOGGER.print(F("Expected 500 bytes, got: "));
    LOGGER.println(strlen(data));
    LOGGER.print(data);
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }
}

// HTTP POST - as a form upload.
void make_http_post() {
  char *data;
  char content_length_ascii[8];

  LOGGER.println(F("Sending test POST to post_echo.php..."));
  
  if (radio.connect_progmem(host, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_post_request);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_post_content_type);
  radio.send_progmem(http_post_content_length);
  // Insert the content length.
  itoa(strlen_P(http_post_content), content_length_ascii, 10);
  radio.send(content_length_ascii);
  radio.send_progmem(http_post_crlf);
  radio.send_progmem(http_close_connection);
  radio.send_progmem(http_post_content);

  data = radio.get_http_response(512);
  if (data) {
    LOGGER.println(F("Got response:"));
    LOGGER.println(data);
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }
}


void setup() {
  // Don't "double start" after code is uploaded.  It confuses the radio.
  delay(2000);
  
  // Power on my radio - you can safely ignore this.
  pinMode(10, INPUT);
  
  // Radio is set, on my hardware, to 9600 baud, on the default pins.
  radio.begin(9600);
  LOGGER.begin(115200);
  
  // If the radio is already in station mode, there's no need to do this every
  // time.
  if (radio.set_station_mode()) {
    LOGGER.println(F("Set station mode success."));
  } else {
    LOGGER.println(F("Set station mode failed."));
  }

  if (radio.connect_to_ap(ssid, password)) {
    LOGGER.println(F("Join AP success."));
  } else {
    LOGGER.println(F("Join AP failed."));
  }

  // Test HTTP get and POST
  make_http_get();
  make_http_get_multiple_packets();
  make_http_post();

  LOGGER.println(F("Done with test code!"));
}

void loop() {
  // No code in loop.
}

