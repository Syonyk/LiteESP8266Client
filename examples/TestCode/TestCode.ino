#include "LiteESP8266Client.h"

// If you have the Logger library, define this to save a lot of SRAM.
#define USE_LITE_SERIAL

#ifdef USE_LITE_SERIAL
#include "LiteSerialLogger.h"
#define LOGGER LiteSerial
#else
#define LOGGER Serial
#endif

// Global instance of the class to work with.
LiteESP8266 radio;

// AP information.
const char ssid[] PROGMEM = "Ferret";
const char password[] PROGMEM = "bilbowasmyfirstferret";

// Domains to test DNS lookup on.
const char google_domain[] PROGMEM = "www.google.com";
const char nonsense_domain[] PROGMEM = "asdfqwejklwd.jihiugwebjk";

// Test host for connections.
const char host[] PROGMEM = "192.168.0.118";

// HTTP Headers
const char http_get_10_bytes_request[]  PROGMEM = 
    "GET /test/get_bytes.php?bytes=10 HTTP/1.0\r\n";
const char http_get_100_bytes_request[] PROGMEM = 
    "GET /test/get_bytes.php?bytes=100 HTTP/1.0\r\n";
const char http_get_500_bytes_request[] PROGMEM = 
    "GET /test/get_bytes.php?bytes=500 HTTP/1.0\r\n";
const char http_get_index[] PROGMEM = "GET / HTTP/1.0\r\n";

const char http_useragent[] PROGMEM = "User-Agent: SyonykArduino/0.1\r\n";
const char http_close_connection[] PROGMEM = "Connection: close\r\n\r\n";



/**
 * Check the "get software version" functionality and print out the versions.
 * 
 * Returns true if this is successful, false on failure.
 */
bool test_software_version() {
  esp8266_version_data software_version;
  if (radio.get_software_version(&software_version)) {
    LOGGER.println(F("Get Software Version success."));
  } else {
    LOGGER.println(F("Get Software Version failed."));
    return false;
  }

  LOGGER.print(F("AT Version: "));
  LOGGER.println(software_version.at_version);
  LOGGER.print(F("SDK Version: "));
  LOGGER.println(software_version.sdk_version);
  LOGGER.print(F("Compile Time: "));
  LOGGER.println(software_version.compile_time);
  return true;
}

/**
 * Test setting different baud rates.  The device should work properly for all
 * of these different baud rates.
 */
bool test_baud_rate() {
  LOGGER.println(F("Testing baud rate setting."));
  
  // Default baud is 9600.  Try setting it higher and lower, then resetting it.
  LOGGER.println(F("Setting baud to 19200."));
  radio.set_radio_baud(19200);
  // Give it a few seconds to work.
  delay(2000);
  LOGGER.println(F("Looking for radio at 19200 baud."));
  if (radio.begin(19200)) {
    LOGGER.println(F("Radio found at 19200 baud."));
  } else {
    LOGGER.println(F("FAILURE: Cannot find radio at 19200 baud."));
    return false;
  }

  // Try 4800.
  LOGGER.println(F("Setting baud to 4800."));
  radio.set_radio_baud(4800);
  // Give it a few seconds to work.
  delay(2000);
  LOGGER.println(F("Looking for radio at 4800 baud."));
  if (radio.begin(4800)) {
    LOGGER.println(F("Radio found at 4800 baud."));
  } else {
    LOGGER.println(F("FAILURE: Cannot find radio at 4800 baud."));
    return false;
  }

  LOGGER.println(F("Setting baud to 9600."));
  radio.set_radio_baud(9600);
  // Give it a few seconds to work.
  delay(2000);
  LOGGER.println(F("Looking for radio at 9600 baud."));
  if (radio.begin(9600)) {
    LOGGER.println(F("Radio found at 9600 baud."));
  } else {
    LOGGER.println(F("FAILURE: Cannot find radio at 9600 baud."));
    return false;
  }

  return true;  
}

bool test_dns_lookup() {
  bool lookups_success = true;
  char ip_address[IP_ADDRESS_LENGTH];

  LOGGER.println(F("Looking up www.google.com."));
  if (radio.dns_lookup_progmem(google_domain, ip_address)) {
    LOGGER.print(F("Got DNS lookup IP:"));
    LOGGER.println(ip_address);
  } else {
    LOGGER.println(F("IP lookup failed."));
    lookups_success = false;
  }
  
  LOGGER.println(F("Looking up what should not exist."));
  if (radio.dns_lookup_progmem(nonsense_domain, ip_address)) {
    LOGGER.print(F("Got DNS lookup IP:"));
    LOGGER.println(ip_address);
    // This should not succeed.
    lookups_success = false;
  } else {
    LOGGER.println(F("IP lookup failed."));
  }
  return lookups_success;
}

/**
 * Sends several requests to a test script, requesting a certain amount of data.
 * 
 * The test script returns that many bytes as data.  This allows verification
 * that all the data is being returned.
 */
bool test_http_data() {
  char *data;

  //  ==== Test 10 bytes ====
  if (radio.connect_progmem(host, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_get_10_bytes_request);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  data = radio.get_http_response(100);
  if (data) {
    LOGGER.print(F("Expected 10 bytes, got: "));
    LOGGER.println(strlen(data));
    // Should be closed, just double checking...
    radio.close();
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }

  //  ==== Test 100 bytes ====
  if (radio.connect_progmem(host, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_get_100_bytes_request);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  data = radio.get_http_response(128);
  if (data) {
    LOGGER.print(F("Expected 100 bytes, got: "));
    LOGGER.println(strlen(data));
    // Should be closed, just double checking...
    radio.close();
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }

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
    // Should be closed, just double checking...
    radio.close();
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }

  //  ==== Test truncated return data ====
  if (radio.connect_progmem(host, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_get_100_bytes_request);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  data = radio.get_http_response(10);
  if (data) {
    LOGGER.print(F("Expected 100 bytes truncated to 9, got: "));
    LOGGER.println(strlen(data));
    // Should be closed, just double checking...
    radio.close();
    free(data);
  } else {
    LOGGER.println(F("Error: Data came back null."));
    return false;
  }

  // Try to get http://www.google.com/
  // Google does not have a content-length header, and has a lot of data.
  if (radio.connect_progmem(google_domain, 80)) {
    LOGGER.println(F("Connect success."));
  } else {
    LOGGER.println(F("Connect failure."));
  }

  radio.send_progmem(http_get_index);
  radio.send_progmem(http_useragent);
  radio.send_progmem(http_close_connection);

  while (data = radio.get_response_packet(1500)) {
    if (data) {
      LOGGER.print(F("Got response packet of size: "));
      LOGGER.println(strlen(data));
      free(data);
    } else {
      LOGGER.println(F("Error: Data came back null."));
    }
  }
  


  return true;
}

void setup() {
  // Delay to avoid the "Double start" problem when uploading code.
  delay(2000);

  // Initialize the radio and Serial or LiteSerialLogger library.
  radio.begin(9600);
  LOGGER.begin(9600);

  LOGGER.println(F("LiteESP8266Client Test Library"));

  // Needed for my hardware.
  pinMode(10, INPUT);
  
  delay(1000);
  // Report the software version.
  test_software_version();
  
  // Test DNS lookups.
  test_dns_lookup();

  // Test the baud rate setting.
  test_baud_rate();

  // Set station mode and try to join an AP.
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

  // Get IP, disconnect, check for IP again.
  char ip_address[IP_ADDRESS_LENGTH];
  radio.get_local_ip(ip_address);
  LOGGER.print(F("Got local IP:"));
  LOGGER.println(ip_address);
  if (radio.disconnect_from_ap()) {
    LOGGER.println(F("Leave AP success."));
  } else {
    LOGGER.println(F("Leave AP failed."));
  }
  radio.get_local_ip(ip_address);
  LOGGER.print(F("Got local IP:"));
  LOGGER.println(ip_address);

  // Reconnect and wait for IP.
  radio.connect_to_ap(ssid, password);

  test_http_data();
  
  LOGGER.println(F("\r\nDone With Testing\r\n=============="));

}

void loop() {
  // If you're not using the LiteSerial library, have a command passthrough!
#ifndef USE_LITE_SERIAL

  while (Serial.available())
    radio.write(Serial.read());
  while (radio.available())
    Serial.write(radio.read());

#endif
}
