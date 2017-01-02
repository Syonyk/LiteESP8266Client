
#include <Arduino.h>

#include "LiteESP8266Client.h"
#include "LiteSerialLogger.h"

// AT test commands and prefix.
const char ESP8266_TEST[] PROGMEM = "AT";  // Test AT startup
const char ESP8266_COMMAND_PREFIX[] PROGMEM = "AT+";
const char ESP8266_COMMAND_DISABLE_ECHO[] PROGMEM = "ATE0";

// AT commands.  ? or = is included after them if needed, and optionally the
// fixed parameters.
const char ESP8266_COMMAND_RESET[] PROGMEM = "RST";
const char ESP8266_COMMAND_VERSION[] PROGMEM = "GMR";
const char ESP8266_COMMAND_DEEP_SLEEP[] PROGMEM = "GSLP=";
const char ESP8266_COMMAND_SET_BAUD[] PROGMEM = "UART_DEF=";
const char ESP8266_COMMAND_SET_RFPOWER[] PROGMEM = "RFPOWER=";
const char ESP8266_COMMAND_SET_STATION_MODE[] PROGMEM = "CWMODE_DEF=1";
const char ESP8266_COMMAND_ENABLE_STATION_DHCP[] PROGMEM = "CWDHCP_DEF=1,1";
const char ESP8266_COMMAND_CONNECT_TO_AP[] PROGMEM = "CWJAP_DEF=";
const char ESP8266_COMMAND_DISCONNET_FROM_AP[] PROGMEM = "CWQAP";
const char ESP8266_COMMAND_DNS_LOOKUP[] PROGMEM = "CIPDOMAIN=";
const char ESP8266_COMMAND_GET_LOCAL_IP[] PROGMEM = "CIFSR";
const char ESP8266_COMMAND_CONNECT[] PROGMEM = "CIPSTART=";
const char ESP8266_COMMAND_CLOSE_CONNECTION[] PROGMEM = "CIPCLOSE";
const char ESP8266_COMMAND_SEND_DATA[] PROGMEM = "CIPSEND=";

// Commands are terminated with CRLF.
const char CRLF[] PROGMEM = "\r\n";

// Serial options beyond baud.  8,N,1 with no flow control is sane.
const char ESP8266_SERIAL_OPTIONS[] PROGMEM = ",8,1,0,0";

// Assorted responses one might look for.
const char ESP8266_RESPONSE_OK[] PROGMEM = "OK\r\n";
const char ESP8266_RESPONSE_ERROR[] PROGMEM = "ERROR\r\n";
const char ESP8266_RESPONSE_FAIL[] PROGMEM = "FAIL\r\n";
const char ESP8266_DNS_LOOKUP_PREFIX[] PROGMEM = "+CIPDOMAIN:";
const char ESP8266_LOCAL_IP_ADDRESS[] PROGMEM = ":STAIP,";
const char ESP8266_SEND_OK[] PROGMEM = "SEND OK\r\n";
const char ESP8266_DATA_PACKET[] PROGMEM = "+IPD,";
const char ESP8266_CONTENT_LENGTH_HEADER[] PROGMEM = "Content-Length: ";

// Terminates HTTP header section, opens content section.
const char ESP8266_CRLFCRLF[] PROGMEM = "\r\n\r\n";

// Connection types - with quotes and commas!
const char ESP8266_TCP[] PROGMEM = "\"TCP\",";
const char ESP8266_UDP[] PROGMEM = "\"UDP\",";
const char ESP8266_SSL[] PROGMEM = "\"SSL\",";

// Set to the maximum command length (above), with terminating null.
#define MAX_COMMAND_LENGTH 16


// =============================================================================
// Onto the code!  Basic radio operations here.
// =============================================================================


LiteESP8266::LiteESP8266() {
  // Ensure radio_serial_ is null - allows detecting if it has been set.
  radio_serial_ = NULL;
}

LiteESP8266::~LiteESP8266() {
  // If radio_serial_ has been allocated, delete it.
  if (radio_serial_) {
    delete radio_serial_;
  }
}

// begin() can be called multiple times during execution to set new baud rates.
bool LiteESP8266::begin(unsigned long baud_rate, byte tx_pin, byte rx_pin) {
  bool radio_is_alive = false;
  
  // Create a SoftwareSerial object if one does not already exist.
  if (!radio_serial_) {
    radio_serial_ = new SoftwareSerial(tx_pin, rx_pin);
  }
  
  // Configure SoftwareSerial to the desired baud rate.
  radio_serial_->begin(baud_rate);

  // Send the "AT" and look for an "OK" response.
  radio_is_alive = test();

  // If the radio is alive, initialize the radio.
  // If successful, this returns true, and the radio is alive and configured.
  if (radio_is_alive) {
    return init_radio();
  }
  return false;
}

bool LiteESP8266::init_radio() {
  return disable_echo();
}

// Sends AT, expects OK.
bool LiteESP8266::test() {
  send_command(ESP8266_TEST);
  return (LITE_ESP8266_SUCCESS ==
          read_for_response(ESP8266_RESPONSE_OK, TEST_TIMEOUT));
}

// Reset the radio.  Radio responds with "OK" then resets.
bool LiteESP8266::reset_radio() {
  send_command_with_prefix(ESP8266_COMMAND_RESET);
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

bool LiteESP8266::disable_echo() {
  send_command(ESP8266_COMMAND_DISABLE_ECHO);
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

bool LiteESP8266::get_software_version(esp8266_version_data *software_version) {
  send_command_with_prefix(ESP8266_COMMAND_VERSION);

  /**
   * Expected results look like this:
   * AT version:1.3.0.0(Jul 14 2016 18:54:01)
   * SDK version:2.0.0(656edbf)
   * compile time:Jul 19 2016 18:43:55
   * OK
   * 
   * Logic to read: Look for the ':' character.  Then read until CRLF (\r\n).
   * Repeat until done, then look for the "OK\r\n" termination.
   * 
   * As copy_serial_to_buffer ensures the strings are null terminated, the
   * structure does not need to be cleared first.
   */

   // Read until the first ':'
   read_until(':');
   // Copy the AT version into the buffer.
   copy_serial_to_buffer(software_version->at_version, '\r', 
           VERSION_STRING_LENGTH);
   // The trailing \n will be consumed looking for the next ':'
   read_until(':');
   copy_serial_to_buffer(software_version->sdk_version, '\r', 
           VERSION_STRING_LENGTH);
   read_until(':');
   copy_serial_to_buffer(software_version->compile_time, '\r', 
           VERSION_STRING_LENGTH);

  // The expected termination string is "OK" - look for it.
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

bool LiteESP8266::deep_sleep_radio(const unsigned long sleep_time_ms) {
  // Max value: 4294967295 (11 with null termination)
  char sleep_time_mills_ascii[11];

  // Convert from unsigned long to an ASCII base-10 representation of this.
  ultoa(sleep_time_ms, sleep_time_mills_ascii, 10);

  // Send the command with the time argument, wait for an "OK" - this comes back
  // before the radio goes to sleep.
  send_command_with_prefix(ESP8266_COMMAND_DEEP_SLEEP, sleep_time_mills_ascii);

  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

bool LiteESP8266::set_radio_baud(const unsigned long baud) {
  // Store the ASCII baud, and then space to append ',8,1,0,0'
  char baud_ascii[19];

  // Convert the ulong baud to ASCII, then append the options from progmem.
  ultoa(baud, baud_ascii, 10);
  strcat_P(baud_ascii, ESP8266_SERIAL_OPTIONS);

  // Send the command with the baud argument, wait for an "OK"
  // Baud changes AFTER the "OK" comes back.
  send_command_with_prefix(ESP8266_COMMAND_SET_BAUD, baud_ascii);
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);  
}

bool LiteESP8266::set_rfpower(const uint8_t rfpower) {
  char rfpower_ascii[4];

  utoa(rfpower, rfpower_ascii, 10);
  
  send_command_with_prefix(ESP8266_COMMAND_SET_RFPOWER, rfpower_ascii);
  
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);  
}

// =============================================================================
// SoftwareSerial passthrough operations.  These allow the user of this class to
// interact with the radio directly if they have a need to.
// =============================================================================

bool LiteESP8266::available() {
  return radio_serial_->available();
}

char LiteESP8266::read() {
  return radio_serial_->read();
}
void LiteESP8266::write(const char c) {
  radio_serial_->write(c);
}

// =============================================================================
// Send commands and look for responses in the SoftwareSerial buffer.
// =============================================================================

void LiteESP8266::send_command(const char* progmem_command,
        const char* params) {
  // Cast the command to call the proper print function for a progmem string.
  radio_serial_->print((__FlashStringHelper*) progmem_command);

  // Send params, if they exist.
  if (params && strlen(params)) {
    radio_serial_->print(params);
  }

  // Send a CRLF to terminate the command.
  radio_serial_->println();
}

void LiteESP8266::send_command_with_prefix(const char *progmem_command,
        const char *params) {
  // Send the "AT+" prefix, then send the rest of the command.
  radio_serial_->print((__FlashStringHelper*) ESP8266_COMMAND_PREFIX);
  send_command(progmem_command, params);
}

uint8_t LiteESP8266::read_for_response(const char* progmem_response_string, 
        const unsigned int timeout_ms) {
  // Index for how many characters have been matched.
  uint8_t matched_chars = 0;
  uint8_t response_length = strlen_P(progmem_response_string);

  // Store the start time for detecting a timeout.
  unsigned long start_time = millis();

  // Loop until the timeout is reached.
  while (millis() < (start_time + timeout_ms)) {
    // Only proceed if a character is available.
    if (radio_serial_->available()) {
      // If the character matches the expected character in the response,
      // increment the pointer.  If not, reset things.
      if (radio_serial_->read() == 
              pgm_read_byte_near(progmem_response_string + matched_chars)) {
        matched_chars++;
   
        if (matched_chars == response_length) {
          return LITE_ESP8266_SUCCESS;
        }
      } else {
        // Character did not match - reset.
        matched_chars = 0;
      }
    }
  }

  // Timeout reached with no match found.
  return LITE_ESP8266_TIMEOUT;  
}

// Same as above, but with two sets of counters/indexes/etc.
uint8_t LiteESP8266::read_for_responses(const char* progmem_pass_string, 
        const char* progmem_fail_string, const unsigned int timeout_ms) {
  uint8_t pass_matched_chars = 0, fail_matched_characters = 0;
  uint8_t pass_response_length = strlen_P(progmem_pass_string);
  uint8_t fail_response_length = strlen_P(progmem_fail_string);

  // Store the start time for timeout purposes.
  unsigned long start_time = millis();

  // Loop until the timout is reached.
  while (millis() < (start_time + timeout_ms)) {
    if (radio_serial_->available()) {
      char next_character = radio_serial_->read();

      // Check and update the "pass" case.
      if (next_character == 
              pgm_read_byte_near(progmem_pass_string + pass_matched_chars)) {
        pass_matched_chars++;
        if (pass_matched_chars == pass_response_length) {
          return LITE_ESP8266_SUCCESS;
        }
      } else {
        pass_matched_chars = 0;
      }

      // Check and update the "fail" case.
      if (next_character == 
            pgm_read_byte_near(progmem_fail_string + fail_matched_characters)) {
        fail_matched_characters++;
        if (fail_matched_characters == fail_response_length) {
          return LITE_ESP8266_FAILURE;
        }
      } else {
        fail_matched_characters = 0;
      }
    }
  }

  // Timeout reached - return timeout.
  return LITE_ESP8266_TIMEOUT;  
}

uint8_t LiteESP8266::copy_serial_to_buffer(char *buffer, 
        const char read_until, const uint16_t max_bytes, 
        const unsigned int timeout_ms) {
  unsigned long start_time = millis();
  uint16_t bytes_read = 0;

  // Loop until timeout.
  while (millis() < (start_time + timeout_ms)) {
    if (radio_serial_->available()) {
      buffer[bytes_read] = radio_serial_->read();

      /**
       * Check to see if the character just read matches the read_until
       * character.  If so, stomp that character with a null terminating byte
       * and return success.
       */
      if (buffer[bytes_read] == read_until) {
        buffer[bytes_read] = 0;
        return LITE_ESP8266_SUCCESS;
      }

      /**
       * Increment bytes_read, and check to see if we're out of space.
       * 
       * If max_bytes is 4, offsets 0, 1, 2 can be used for characters, but
       * offset 3 must be saved for the null terminator.
       * 
       * If the length is exceeded, null terminate what has been read and return
       * the proper error.  Note that the remaining data in the serial buffer is
       * NOT read - that can be read by the calling code again, if they wish.
       */
      bytes_read++;

      if (bytes_read >= (max_bytes - 1)) {
        buffer[bytes_read] = 0;
        return LITE_ESP8266_LENGTH_EXCEEDED;
      }
    }
  }
  
  // Timeout reached - return timeout.
  return LITE_ESP8266_TIMEOUT;  
}

uint8_t LiteESP8266::read_until(const char read_until, 
        const unsigned int timeout_ms) {
  unsigned long start_time = millis();

  while (millis() < (start_time + timeout_ms)) {
    if (radio_serial_->available()) {
      // If the character matches the expected termination character, return.
      if (read_until == radio_serial_->read()) {
        return LITE_ESP8266_SUCCESS;
      }
    }
  }

  return LITE_ESP8266_TIMEOUT;  
}

// =============================================================================
// Wireless commands - related to connecting to an AP.
// =============================================================================

bool LiteESP8266::set_station_mode() {
  bool success = false;
  // Set the radio to station mode.
  send_command_with_prefix(ESP8266_COMMAND_SET_STATION_MODE);
  // If it succeeded, set success to true.
  (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS) ? 
    success = true : success = false;
  
  // Enable client DHCP.
  send_command_with_prefix(ESP8266_COMMAND_ENABLE_STATION_DHCP);
  // If both command succeeded, return true.
  return (success && (read_for_response(ESP8266_RESPONSE_OK) == 
          LITE_ESP8266_SUCCESS));
}

bool LiteESP8266::connect_to_ap(const char *progmem_ssid, 
        const char *progmem_password, const char *progmem_bssid) {
  // AP SSID may be 32 characters.
  // Password may be 64 characters.
  // BSSID is 17.  Plus all this needs quotes.
  char join_ap_buffer[128];

  memset(join_ap_buffer, 0, sizeof(join_ap_buffer));

  // Add the opening quote for the SSID.
  join_ap_buffer[0] = '"';

  // Append the SSID.
  strcat_P(join_ap_buffer, progmem_ssid);

  // Apend the closing quote.
  join_ap_buffer[strlen(join_ap_buffer)] = '"';

  // If there is a password, append it.
  if (progmem_password) {
    // Comma, opening quote, password, close quote.
    join_ap_buffer[strlen(join_ap_buffer)] = ',';
    join_ap_buffer[strlen(join_ap_buffer)] = '"';
    strcat_P(join_ap_buffer, progmem_password);
    join_ap_buffer[strlen(join_ap_buffer)] = '"';
  }

  if (progmem_bssid) {
    // Comma, opening quote, BSSID, close quote.
    join_ap_buffer[strlen(join_ap_buffer)] = ',';
    join_ap_buffer[strlen(join_ap_buffer)] = '"';
    strcat_P(join_ap_buffer, progmem_bssid);
    join_ap_buffer[strlen(join_ap_buffer)] = '"';
  }

  // Join AP either ends in OK or FAIL.
  send_command_with_prefix(ESP8266_COMMAND_CONNECT_TO_AP, join_ap_buffer);

  return (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_FAIL, WIFI_CONNECT_TIMEOUT));
}

bool LiteESP8266::disconnect_from_ap() {
  send_command_with_prefix(ESP8266_COMMAND_DISCONNET_FROM_AP);
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

// =============================================================================
// IP Status and DNS Lookup Commands
// =============================================================================

/**
 * On success, a DNS lookup returns something like the following:
 * +CIPDOMAIN:216.58.216.142
 *
 * OK
 * 
 * On failure, it returns this:
 * DNS Fail
 *
 * ERROR
 * 
 * So, the "success" string is +CIPDOMAIN:, and the failure string is ERROR.
 * If successful, swallow the IP into the ip_address string.
 */
bool LiteESP8266::dns_lookup(const char *domain, char *ip_address) {
  // Because the domain needs to be quoted, send the command manually to avoid
  // needing a large buffer allocated in SRAM.
  radio_serial_->print((__FlashStringHelper*) ESP8266_COMMAND_PREFIX);
  radio_serial_->print((__FlashStringHelper*) ESP8266_COMMAND_DNS_LOOKUP);
  // Sending single characters doesn't use SRAM space.
  radio_serial_->print('"');
  radio_serial_->print(domain);
  radio_serial_->print('"');
  // Send a CRLF to terminate the command.
  radio_serial_->println();

  // DNS can take a while - give it 30s.
  if (read_for_responses(ESP8266_DNS_LOOKUP_PREFIX, ESP8266_RESPONSE_ERROR, 
          WIFI_CONNECT_TIMEOUT) == LITE_ESP8266_SUCCESS) {
    // Success - read the IP and return.
    copy_serial_to_buffer(ip_address, '\r', IP_ADDRESS_LENGTH);
    
    // There's an OK\r\n after this - swallow that and report success.
    read_for_response(ESP8266_RESPONSE_OK);
    return true;
  }
  
  // Not successful.  Return false.
  return false;
}

// Same as above, but the domain is in PROGMEM.
bool LiteESP8266::dns_lookup_progmem(const char *progmem_domain, 
        char *ip_address) {
  radio_serial_->print((__FlashStringHelper*) ESP8266_COMMAND_PREFIX);
  radio_serial_->print((__FlashStringHelper*) ESP8266_COMMAND_DNS_LOOKUP);
  radio_serial_->print('"');
  radio_serial_->print((__FlashStringHelper*)progmem_domain);
  radio_serial_->print('"');
  radio_serial_->println();

  if (read_for_responses(ESP8266_DNS_LOOKUP_PREFIX, ESP8266_RESPONSE_ERROR, 
          WIFI_CONNECT_TIMEOUT) == LITE_ESP8266_SUCCESS) {
    copy_serial_to_buffer(ip_address, '\r', IP_ADDRESS_LENGTH);
    
    read_for_response(ESP8266_RESPONSE_OK);
    return true;
  }
  
  return false;
}

/*
 * Response:
 * +CIFSR:STAIP,"192.168.0.120"
 * +CIFSR:STAMAC,"18:fe:34:9f:bb:18"
 * 
 * OK
 */
bool LiteESP8266::get_local_ip(char *ip_address) {
  send_command_with_prefix(ESP8266_COMMAND_GET_LOCAL_IP);

  // Read until the first quote.
  read_for_response(ESP8266_LOCAL_IP_ADDRESS);
  read_until('"');
  // Copy the IP in the buffer - terminated by another quote.
  copy_serial_to_buffer(ip_address, '"', IP_ADDRESS_LENGTH);
  
  // Check for OK and swallow MAC address response.
  return (read_for_response(ESP8266_RESPONSE_OK) == LITE_ESP8266_SUCCESS);
}

// =============================================================================
// Connect, send, and receive data from a remote endpoint.
// =============================================================================

bool LiteESP8266::connect_progmem(const char *progmem_host, 
        const unsigned int port, const uint8_t protocol) {
  char connect_buffer[128];
  char port_to_ascii[6];

  memset(connect_buffer, 0, sizeof(connect_buffer));

  // Default or unknown is TCP.
  // This inserts "TCP", into the buffer.
  switch (protocol) {
    default:
    case LITE_ESP8266_TCP:
      strcpy_P(connect_buffer, ESP8266_TCP);
      break;
    case LITE_ESP8266_UDP:
      strcpy_P(connect_buffer, ESP8266_UDP);
      break;
    case LITE_ESP8266_SSL:
      strcpy_P(connect_buffer, ESP8266_SSL);
      break;
  }

  // Insert the host, quoted.
  connect_buffer[strlen(connect_buffer)] = '"';
  strcat_P(connect_buffer, progmem_host);  
  connect_buffer[strlen(connect_buffer)] = '"';
  connect_buffer[strlen(connect_buffer)] = ',';
  // Make the port an ASCII string and append it.
  utoa(port, port_to_ascii, 10);
  strcat(connect_buffer, port_to_ascii);

  send_command_with_prefix(ESP8266_COMMAND_CONNECT, connect_buffer);
  return (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_ERROR, CLIENT_CONNECT_TIMEOUT));
}

bool LiteESP8266::connect(const char *host, const unsigned int port, 
        const uint8_t protocol) {
  char connect_buffer[128];
  char port_to_ascii[6];

  memset(connect_buffer, 0, sizeof(connect_buffer));

  // Default or unknown is TCP.
  // This inserts "TCP", into the buffer.
  switch (protocol) {
    default:
    case LITE_ESP8266_TCP:
      strcpy_P(connect_buffer, ESP8266_TCP);
      break;
    case LITE_ESP8266_UDP:
      strcpy_P(connect_buffer, ESP8266_UDP);
      break;
    case LITE_ESP8266_SSL:
      strcpy_P(connect_buffer, ESP8266_SSL);
      break;
  }

  // Insert the host.
  connect_buffer[strlen(connect_buffer)] = '"';
  strcat(connect_buffer, host);  
  connect_buffer[strlen(connect_buffer)] = '"';
  connect_buffer[strlen(connect_buffer)] = ',';
  // Make the port an ASCII string and append it.
  utoa(port, port_to_ascii, 10);
  strcat(connect_buffer, port_to_ascii);

  send_command_with_prefix(ESP8266_COMMAND_CONNECT, connect_buffer);
  return (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_ERROR, CLIENT_CONNECT_TIMEOUT));
}

bool LiteESP8266::close() {
  send_command_with_prefix(ESP8266_COMMAND_CLOSE_CONNECTION);
  return (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_ERROR));
}

bool LiteESP8266::send(const char *data) {
  char length_buffer[6];

  // Get the data length, in ASCII.
  utoa(strlen(data), length_buffer, 10);

  // Attempt to send the data - send a request to send a given length.
  send_command_with_prefix(ESP8266_COMMAND_SEND_DATA, length_buffer);
  // Check for OK or ERROR response.
  if (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_ERROR)) {
    // Success - send the data!
    radio_serial_->print(data);
  } else {
    // Something went wrong.  Send is not successful.
    return false;
  }

  // Look for "SEND OK" response.
  return (LITE_ESP8266_SUCCESS == read_for_response(ESP8266_SEND_OK));
}

bool LiteESP8266::send_progmem(const char *data) {
  char length_buffer[6];

  utoa(strlen_P((const char *)data), length_buffer, 10);

  // Attempt to send the data.
  send_command_with_prefix(ESP8266_COMMAND_SEND_DATA, length_buffer);
  // Check for OK or ERROR response.
  if (LITE_ESP8266_SUCCESS == read_for_responses(ESP8266_RESPONSE_OK, 
          ESP8266_RESPONSE_ERROR)) {
    // Success - send the data!  Cast first to call the proper function.
    radio_serial_->print((const __FlashStringHelper *)data);
  } else {
    // Something went wrong.  Send is not successful.
    return false;
  }
  // Look for "SEND OK" response.
  return (LITE_ESP8266_SUCCESS == read_for_response(ESP8266_SEND_OK));
}

// Response comes back like this:
// +IPD,532:<data>
char *LiteESP8266::get_response_packet(const unsigned int max_allocate_bytes, 
        const unsigned int timeout_ms) {
  // Can get up to 2048 bytes of response packet, though you can't fit that in
  // Arduino Uno SRAM.  Realistically, data size is likely to be about 1430.
  char data_length_buffer[5];
  char *data;
  unsigned long start_time = millis();

  // Read until '+IPD,"
  if (read_for_response(ESP8266_DATA_PACKET, CLIENT_CONNECT_TIMEOUT) == 
          LITE_ESP8266_SUCCESS) {
    unsigned int data_length, bytes_allocated;
    
    // '+IPD,' found - get the data length and proceed.
    copy_serial_to_buffer(data_length_buffer, ':', sizeof(data_length_buffer));
    data_length = atoi(data_length_buffer);

    // Allocate space - either the data length, or the max allowed bytes.
    // Include space for the null terminator character.
    if (max_allocate_bytes > data_length) {
      data = (char *)malloc(data_length + 1);
      bytes_allocated = data_length + 1;
    } else {
      data = (char *)malloc(max_allocate_bytes);
      bytes_allocated = max_allocate_bytes;
    }

    memset(data, 0, bytes_allocated);
    
    for (unsigned int i = 0; i < data_length; i++) {
      // Loop until data is ready, unless the timeout is exceeded.
      while (!radio_serial_->available() &&
              (millis() < (start_time + timeout_ms)));
      // If the timeout is exceeded, break.
      if (millis() > (start_time + timeout_ms)) {
        break;
      }
      // Only copy the data if there is enough space.
      if (i < (bytes_allocated - 1)) {
        // Copy the data into the buffer.
        data[i] = radio_serial_->read();
      } else {
        // Just read and discard the overrunning data.
        radio_serial_->read();
      }
    }
    return data;
  }
  // No +IPD found - return null.
  return NULL;
}

// Similar to above, but only returns the actual HTTP response.
// Note: This is not using the above to avoid double allocating memory.
char *LiteESP8266::get_http_response(const unsigned int max_allocate_bytes, 
        const unsigned int timeout_ms) {
  char content_length_buffer[16];
  char *data;
  unsigned long start_time = millis();

  // Read until the Content-Length: header.
  if (read_for_response(ESP8266_CONTENT_LENGTH_HEADER, CLIENT_CONNECT_TIMEOUT) 
          == LITE_ESP8266_SUCCESS) {
    unsigned int content_length, bytes_allocated;
    
    // Read until the end of the line for the number of bytes to read.
    copy_serial_to_buffer(content_length_buffer, '\r', 
            sizeof(content_length_buffer));
    content_length = atoi(content_length_buffer);

    // Read for CRLFCRLF - this terminates the response header.
    if (read_for_response(ESP8266_CRLFCRLF, CLIENT_CONNECT_TIMEOUT) == 
            LITE_ESP8266_SUCCESS) {
      // Found it - next content_length bytes are data!
      if (max_allocate_bytes > content_length) {
        data = (char *)malloc(content_length + 1);
        bytes_allocated = content_length + 1;
      } else {
        data = (char *)malloc(max_allocate_bytes);
        bytes_allocated = max_allocate_bytes;
      }

      memset(data, 0, bytes_allocated);
      for (unsigned int i = 0; i < content_length; i++) {
        // Loop until data is ready, unless the timeout is exceeded.
        while (!radio_serial_->available() &&
                (millis() < (start_time + timeout_ms)));
        // If the timeout is exceeded, break.
        if (millis() > (start_time + timeout_ms)) {
          break;
        }
        // Copy the data into the buffer.
        // Only copy the data if there is enough space.
        if (i < (bytes_allocated - 1)) {
          // Copy the data into the buffer.
          data[i] = radio_serial_->read();
        } else {
          // Just read and discard the overrunning data.
          radio_serial_->read();
        }
      }
      return data;
    }
  }
  // No Content-Length: header found!
  return NULL;
}

