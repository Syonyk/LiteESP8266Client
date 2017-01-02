/**
 * This is a lightweight, low-SRAM ESP8266 client for small projects.
 *
 * It uses 100 bytes of global SRAM.  Pair this with the LiteSerialLogger for
 * output, and you can have both serial logging and ESP8266 AT communications
 * with only 109 bytes of global memory used.  Compare this to some other
 * ESP8266 libraries (with the Serial class enabled), and you'll see the
 * savings.
 *
 * This library does not include everything.  It is designed for a simple,
 * single connection client.
 *
 * It only supports station mode.  If you need an AP, use a different
 * library.
 *
 * It only supports software serial.
 *
 * If you extend it, please send patches!
 */


#ifndef _LITEESP8266CLIENT_H_
#define _LITEESP8266CLIENT_H_

// Basic include files - both are needed.
#include <Arduino.h>
#include <SoftwareSerial.h>

/**
 * Default software serial TX/RX pins.  This matches the SparkFun shield and
 * library, and happens to be what this library author uses as well for his
 * shields.
 *
 * I suggest hooking things up this way:
 * ESP8266 TX -> D8
 * ESP8266 RX -> D9
 */
#define ESP8266_SW_TX 8
#define ESP8266_SW_RX 9

/**
 * Timeouts for various operations (in ms).  Note that, in many cases, if the
 * radio is busy, simply waiting will resolve things.  This is why the
 * TEST_TIMEOUT is 10s - this seems to be long enough to wait for most of the
 * issues to clear out.  The radio simply doesn't respond to serial commands if
 * busy.
 */
#define COMMAND_RESPONSE_TIMEOUT 1000
#define COMMAND_PING_TIMEOUT 3000
#define WIFI_CONNECT_TIMEOUT 30000
#define COMMAND_RESET_TIMEOUT 5000
#define CLIENT_CONNECT_TIMEOUT 5000
#define TEST_TIMEOUT 10000

/**
 * Response codes from various functions.
 *
 * Successful execution is always 0, with higher values indicating various
 * failures.
 */
#define LITE_ESP8266_SUCCESS 0
#define LITE_ESP8266_FAILURE 1
#define LITE_ESP8266_TIMEOUT 2
#define LITE_ESP8266_LENGTH_EXCEEDED 3

// Connection type defines, off in their own part of int space.
#define LITE_ESP8266_TCP 100
#define LITE_ESP8266_UDP 101
#define LITE_ESP8266_SSL 102

/**
 * This define and structure are used for storing and returning the radio
 * version strings.
 *
 * AT+GMR returns something like this:
 * AT version:1.3.0.0(Jul 14 2016 18:54:01)
 * SDK version:2.0.0(656edbf)
 * compile time:Jul 19 2016 18:43:55
 *
 * The bits after the ':' will be returned in the appropriate fields as null
 * terminated strings.
 */
#define VERSION_STRING_LENGTH 32

typedef struct {
  char at_version[VERSION_STRING_LENGTH];
  char sdk_version[VERSION_STRING_LENGTH];
  char compile_time[VERSION_STRING_LENGTH];
} esp8266_version_data;

// An IPv4 address requires a string of 16 bytes.
// 255.255.255.255\0 (null terminator).
#define IP_ADDRESS_LENGTH 16

class LiteESP8266 {
public:
  LiteESP8266();
  ~LiteESP8266();

  // ===========================================================================
  // Basic radio management functions
  // ===========================================================================

  /**
   * Initialize the class.  This is a separate function called after creation as
   * this can fail if the radio is not present or is locked up.
   *
   * You may call begin multiple times to change the baud rate, but it will only
   * set the TX/RX pins the first time called - if you need to change the pins,
   * you will need to delete the class and reinstantiate it.
   *
   * @param baud_rate The baud rate of the radio interface.  As this is a
   *  software serial interface, 9600 or 19200 are suggested.
   * @param tx_pin The transmit pin for software serial communication.
   * @param rx_pin The receive pin for software serial communication.
   * @return True if the radio is initialized properly, otherwise false.
   */
  bool begin(unsigned long baud_rate = 9600, byte tx_pin = ESP8266_SW_TX,
          byte rx_pin = ESP8266_SW_RX);

  /**
   * Sends an "AT\r\n" string to the radio and looks for an "OK\r\n" response.
   *
   * If this test is successful, the radio is properly configured and accepting
   * commands, so other commands can proceed.  If this is NOT successful, either
   * the radio is not hooked up properly, the baud rate is wrong, the radio is
   * turned off, etc.  Troubleshoot!
   *
   * @return True if the radio responds to AT with OK.
   */
  bool test();

  /**
   * Initialize the radio.  This should be called after powering the radio on -
   * you do NOT need to call begin() again if you've powered the radio off, as
   * the software serial behavior is the same.
   *
   * @return True on success, false on failure.
   */
  bool init_radio();

  /**
   * Sends "AT+RST\r\n" to the radio, waits for an "OK" response.  This will
   * reset the radio in much the same manner as a power cycle.  You should wait
   * 5-10 seconds after calling this before expecting the radio to be online
   * and responding to commands.
   *
   * @return True if the radio responded OK.
   */
  bool reset_radio();

  /**
   * Sends "AT+GMR\r\n" to the radio, parses the response into the caller-
   * allocated esp8266_version_data structure.
   *
   * The string version and build time data is returned in this structure.
   *
   * Note that the structure requires 96 bytes of SRAM, so calling this when
   * memory is tight is a bad idea.
   *
   * @return True if the radio responded with OK after the data and the data was
   *   properly inserted into the structure, false if something went wrong.
   */
  bool get_software_version(esp8266_version_data *software_version);

  /**
   * Deep sleep the radio for the requested number of milliseconds.
   *
   * NOTE: THE RADIO WILL NOT WAKE UP UNLESS YOU CONNECT XPD_DCDC TO EXT_RSTB
   *
   * Seriously.  I will laugh at you if you file a bug for this and your
   * hardware isn't properly modified for this to work.
   *
   * Deep sleep effectively turns the radio off entirely.
   *
   * @param sleep_time_ms Time to sleep, in milliseconds.
   * @return True if the command returned OK, which will happen shortly before
   *   the radio goes into a deep sleep.
   */
  bool deep_sleep_radio(const unsigned long sleep_time_ms);

  /**
   * Set the radio baud rate and store it to flash.
   *
   * Software serial is unreliable past about 19200 baud, in my experience.
   * So while this will let you push the baud rate, you may or may not be able
   * to communicate with the radio afterwards.
   *
   * If you do change the baud, you'll need to call "begin" again - this is safe
   * to call multiple times, and will set the baud each time.
   *
   * @param baud The baud rate to send to the radio.  It would be wise to ensure
   *   that the baud rate is both supported and within the range of software
   *   serial.
   * @return True on success.  After returning true, the baud rate will change.
   */
  bool set_radio_baud(const unsigned long baud);

  // Set transmit RF power.  Range: 0-82, 0.25dBm increments.
  bool set_rfpower(const uint8_t rfpower);


  /**
   * Configure the radio to a normal station mode operation.  This sets the
   * radio to station mode (connect to an access point instead of being an AP),
   * and enables client DHCP, which is probably what you want.  This library
   * currently doesn't support explicitly setting client IPs.  Add it if you'd
   * like it!
   *
   * @return True if everything went properly.
   */
  bool set_station_mode();

  /**
   * Connect to an access point with the specified parameters
   *
   * progmem_ssid: The SSID, stored in progmem.
   * progmem_password: The password, stored in progmem.
   * progmem_bssid: The MAC address of the AP, if multiple are visible - you
   *   guessed it, in progmem!
   *
   * You'll want to do something like this for your parameters.  These don't
   * use data memory, and anything in data memory won't get read properly.
   * const char ssid[] PROGMEM = "MySSID";
   *
   * Note that if you have weird characters (like commas) in your SSID, they
   * need to be escaped by YOU.
   *
   * If SSID is "ab\, c" and password is "0123456789"\"
   * AT+CWJAP_CUR="ab\\\, c", "0123456789\"\\"
   *
   * If you want to use bssid on an open AP, you'll need to pass in a null
   * string as the password (not NULL).
   *
   * In the interests of simplicity, this doesn't return much in the way of
   * details about why the connection failed - you should be able to figure it
   * out if needed.
   *
   * @param progmem_ssid SSID, stored in program memory.
   * @param progmem_password Preshared key, stored in program memory.
   * @param progmem_bssid MAC address of the AP, stored in program memory.
   * @return True if the AP is joined successfully, else false.
   */
  bool connect_to_ap(const char *progmem_ssid,
          const char *progmem_password = NULL,
          const char *progmem_bssid = NULL);

  /**
   * Disconnect from the AP.
   *
   * If you just power the radio down or put it to sleep, the AP will eventually
   * figure out that it's gone, but it's nice to tell the AP you're going to go
   * away before actually going away.
   */
  bool disconnect_from_ap();

  // ===========================================================================
  // TCP/IP related functions
  // ===========================================================================

  /**
   * DNS lookup.  The DNS string comes out of program memory or data memory,
   * and the char *ip_address must be at least IP_ADDRESS_LENGTH bytes long, or
   * things will overflow.
   *
   * @param domain The domain, as a string in the data region.
   * @param progmem_domain The domain, as a string in the progmem region.
   * @param ip_address A pointer to a char string of at least IP_ADDRESS_LENGTH
   *   bytes long, which will contain the null terminted IP address in string
   *   form if the lookup is successful.
   * @return True if the lookup succeeded, false otherwise.
   */
  bool dns_lookup(const char *domain, char *ip_address);
  bool dns_lookup_progmem(const char *progmem_domain, char *ip_address);

  /**
   * Get the local station IP address.  If everything is set up properly, this
   * should be something on your local subnet...
   *
   * An IP of 0.0.0.0 means that no valid IP is assigned.
   *
   * You need a pointer to a char region of at least IP_ADDRESS_LENGTH bytes.
   * The IP address is returned, null terminated, in this string.
   *
   * @param ip_address A pointer to a char string of at least IP_ADDRESS_LENGTH
   *   bytes.
   * @return True if the IP (even a null IP) is found, false otherwise.
   */
  bool get_local_ip(char *ip_address);

  /**
   * Connect to a remote IP/port.  The remote host should be stored in program
   * memory as a string.
   *
   * This is intended to be used for TCP, but may work with UDP or SSL
   * connection types as well.
   *
   * Returns true on successful connect, otherwise false.
   *
   * @param progmem_host The remote host (IP or DNS name) in progmem.
   * @param port The remote port to connect to.
   * @param progmem_protocol The protocol to use.  Defaults to TCP.
   * @return True if the connection is successful, false if it fails.
   */
  bool connect_progmem(const char *progmem_host,
          const unsigned int port,
          const uint8_t protocol = LITE_ESP8266_TCP);

  // Same thing as above, but with the host stored in data memory.
  bool connect(const char *host,
          const unsigned int port,
          const uint8_t protocol = LITE_ESP8266_TCP);

  /**
   * Close the connection if one is open.  You can call it all you want with
   * no open connection, but it's not going to do much...
   *
   * @return True if the connection was closed, false if there was an error.
   */
  bool close();

  /**
   * Send data through an open connection.
   *
   * These functions require an open connection, and will send data to the
   * remote endpoint.  One works with data stored in program memory, one works
   * with a string stored in data memory.
   *
   * The strings need to be properly null terminated.
   *
   * @param data The data to send, either in program memory or data memory.
   * @return True if the data is sent successfully.
   */
  bool send(const char *data);
  bool send_progmem(const char *data);

  /**
   * Get a response packet.  This is from the "+IPD,<len>:" on - so includes all
   * the HTTP headers and such.
   *
   * This will read a full packet of data from the ESP8266, which can be up to
   * about 2048 bytes.  Unfortunately, the Arduino doesn't have enough SRAM to
   * fit this (if you're using an Uno), so you need to tell it how many bytes to
   * allocate, max.  If the data size is more than that, it will copy the first
   * bytes into the buffer, and read the rest out and discard them.
   *
   * IMPORTANT: THIS RETURNS A MALLOC'D STRING AND YOU MUST FREE IT.
   * If there is an error, the return value is NULL - so don't go assuming you
   * have a string unless you check.
   *
   * char *response = get_response_packet(...);
   * if (response) {
   *   // Do stuff
   *   free(response);
   * }
   *
   * If you do not do that, you will leak memory, badly, and your program will
   * die quickly.
   *
   * @param max_allocate_bytes The maximum allowed number of bytes to allocate
   *   for the response.
   * @param timeout_ms The time to wait for the full response packet.
   * @return A character buffer, filled with either the full packet, as much as
   *   could be read before the timeout, or max_allocate_bytes - 1 characters,
   *   null terminated.  THE CALLER MUST FREE THIS BUFFER.
   *   If something has gone wrong, the return is NULL - check this!
   */
  char *get_response_packet(const unsigned int max_allocate_bytes,
          const unsigned int timeout_ms = CLIENT_CONNECT_TIMEOUT);

  /**
   * Same as above, but works with HTTP response headers, and only returns
   * the content after the header.
   *
   * This function requires a header with the Content-length field, and will
   * start copying data after the \r\n\r\n that terminates the header.
   *
   * Again, you MUST FREE THE RESPONSE.  It's dynamically allocated, and may be
   * NULL if the header is not found.
   *
   * Sample header:
   *
   * HTTP/1.1 200 OK
   * Date: Sat, 24 Dec 2016 20:33:00 GMT
   * Server: Apache/2.4.10 (Raspbian)
   * Vary: Accept-Encoding
   * Content-Length: 339
   * Connection: close
   * Content-Type: text/html; charset=UTF-8
   *
   * @param max_allocate_bytes The maximum allowed number of bytes to allocate
   *   for the response.
   * @param timeout_ms The time to wait for the full response packet.
   * @return A character buffer, filled with either the data after headers, as
   *   much as could be read before the timeout, or max_allocate_bytes - 1
   *   characters, null terminated.  THE CALLER MUST FREE THIS BUFFER.
   *   If something has gone wrong, the return is NULL - check this!
   */
  char *get_http_response(const unsigned int max_allocate_bytes,
          const unsigned int timeout_ms = CLIENT_CONNECT_TIMEOUT);


  /**
   * These functions are available to calling code to enable adding new
   * functions easily.  These are straight passthroughs to the SoftwareSerial
   * functions of the same name, and allow the user of this class to directly
   * read data from the radio, write data, etc.
   */
  bool available();
  char read();
  void write(const char c);

protected:
  // Pointer to the SoftwareSerial object used to talk to the radio.
  // Should be about 4 bytes of SRAM.
  SoftwareSerial* radio_serial_;

private:
  /**
   * Disables command echo - "ATE0\r\n"
   * 
   * This prevents the radio from echoing back the commands.  There is no reason
   * to do that with this library, and it simply wastes space in the buffer that
   * could be used for something useful.
   * 
   * @return True if the command was successful.
   */
  bool disable_echo();

  /**
   * Send a command to the radio.  This requires the command to be stored in
   * program memory.  It will send the command, with any params appended, then
   * \r\n to terminate the command.  If params is NULL, then nothing is appended
   * beyond the primary command (useful for things like a bare AT).
   * 
   * Does not check for any response - other code does that.  This just bangs it
   * out on the software serial port.
   * 
   * @param progmem_command The command to send, in program memory.
   * @param params NULL if empty, or a data memory string of parameters to send.
   */
  void send_command(const char* progmem_command, const char* params = NULL);

  /**
   * Send a command to the radio, prefixed with the "AT+" command prefix.  This
   * sends "AT+", the command, and the params, if not null.  The command must be
   * in program memory.
   * 
   * @param progmem_command The command to send, in program memory, prefixed
   *   with "AT+"
   * @param params NULL if empty, or a data memory string of parameters to send.
   */
  void send_command_with_prefix(const char *progmem_command, 
                                const char *params = NULL);

  /**
   * Read serial output, matching it character for character until either the
   * desired response string is found or the read times out.
   * 
   * This replaces the serial read buffer in other implementations with a bit
   * of more complex code, relying on the SoftwareSerial buffer to contain the
   * data while it's being read.  At lower baud rates doable with the software
   * serial interface, this is quite easy.
   * 
   * read_for_response looks for the response string (from program memory) and
   * returns success if this is found, otherwise will return a timeout error
   * when the timeout is reached.
   * 
   * This is useful for two things: Verifying that a command completed with the
   * desired response, and reading until a particular string is found.
   * 
   * After success, the entire progmem_response_string will be consumed from the
   * buffer, with the head of the buffer being the next character after.
   * 
   * @param progmem_response_string The string, in program memory, to look for
   *   in the output.
   * @param timeout_ms The maximum time, in ms, to wait for a response before
   *   returning a timeout failure.
   * @return Either LITE_ESP8266_SUCCESS or LITE_ESP8266_TIMEOUT, depending on
   *   which condition was met.
   */
  uint8_t read_for_response(const char* progmem_response_string,
          const unsigned int timeout_ms = COMMAND_RESPONSE_TIMEOUT);

  /**
   * Similar to above, but can read for both a success and a failure string for
   * commands that return one of two responses.
   * 
   * If progmem_pass_string is found, LITE_ESP8266_SUCCESS is returned.
   * 
   * If progmem_fail_string is found, LITE_ESP8266_FAILURE is returned.
   * 
   * And, if neither string is found by the timeout, LITE_ESP8266_TIMEOUT is
   * returned.
   * 
   * @param progmem_pass_string The "success" string, in program memory.
   * @param progmem_fail_string The "failure" string, in program memory.
   * @param timeout_ms The maximum time, in ms, to wait for a response before
   *   returning a timeout failure.
   * @return LITE_ESP8266_SUCCESS, _FAILURE, or _TIMEOUT, as appropriate.
   */
  uint8_t read_for_responses(const char* progmem_pass_string,
          const char* progmem_fail_string, 
          const unsigned int timeout_ms = COMMAND_RESPONSE_TIMEOUT);

  /**
   * copy_serial_to_buffer does what the name implies.  It copies the serial 
   * output to the buffer until either the read_until character is found,
   * max_bytes is copied, or the timeout is met.  The caller must allocate an
   * appropriate buffer and offer the size of it to prevent overwriting the end.
   *
   * If everything fits, this function will copy all data up to, but not
   * including, the read_until character, into the buffer.  However, the
   * read_until character will be removed from the serial buffer.
   * 
   * Ex: abcdefg
   * If read_until is 'd', the returned string will be 'abc' and the remaining
   * serial buffer will be 'efg' - d is consumed.
   * 
   * This function ensures that buffer is null terminated, and fits in the
   * size provided by max_bytes.  It will stop reading if the buffer is full.
   * 
   * If you find a need to read binary data of a certain length, use the read()
   * function to read data directly from the serial output - this function
   * assumes a terminating character of some variety.
   * 
   * Note that strings coming back from the ESP8266 are terminated with \r\n.
   * 
   * Possible return values:
   * LITE_ESP8266_SUCCESS: Everything fit and read_until was found.
   * LITE_ESP8266_LENGTH_EXCEEDED: max_bytes of data (null terminated) were
   *   inserted, but read_until was not found in the output.
   * LITE_ESP8266_TIMEOUT: Timeout exceeded.
   *
   * @param buffer A char buffer of at least max_bytes length.
   * @param read_until The character to terminate reading with.  Often \r.
   * @param max_bytes The maximum number of bytes to read.
   * @param timeout_ms The timeout, in milliseconds.
   * @return Success or failure code.  See above for more detail.
   */
  uint8_t copy_serial_to_buffer(char *buffer,
                      const char read_until,
                      const uint16_t max_bytes, 
                      const unsigned int timeout_ms = COMMAND_RESPONSE_TIMEOUT);

  /**
   * Read serial until a certain character is found, or the timeout occurs.
   * 
   * If you wish to read for a string, use read_for_response and look for an
   * LITE_ESP8266_SUCCESS response.
   *
   * @param read_until The character to read until.
   * @param timeout_ms Timeout, in milliseconds, to wait for that character.
   * @return LITE_ESP8266_SUCCESS or LITE_ESP8266_TIMEOUT.
   */
  uint8_t read_until(const char read_until, 
                     const unsigned int timeout_ms = COMMAND_RESPONSE_TIMEOUT);

};

#endif // _LITEESP8266CLIENT_H_

