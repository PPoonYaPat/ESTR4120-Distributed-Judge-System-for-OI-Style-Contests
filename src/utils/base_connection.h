#ifndef BASE_CONNECTION_H
#define BASE_CONNECTION_H

#include <cstdint>
#include <string>
#include <vector>
#include <netinet/in.h>

using namespace std;

// ============================================================================
// Low-level socket operations
// ============================================================================

// Send exactly len bytes (loops until all bytes are sent)
void send_all(int socket, const void* data, size_t len);

// Receive exactly len bytes (loops until all bytes are received)
void recv_all(int socket, void* data, size_t len);

// ============================================================================
// Integer send/receive with network byte order conversion
// ============================================================================

// Send a 32-bit integer (converts to network byte order)
void send_int(int socket, int value);

// Receive a 32-bit integer (converts from network byte order)
int recv_int(int socket);

// ============================================================================
// String send/receive (sends length first, then data)
// ============================================================================

// Send a string (sends length as int, then string data)
void send_string(int socket, const string& str);

// Receive a string (receives length first, then string data)
string recv_string(int socket);

// ============================================================================
// Binary data send/receive (sends length first, then data)
// ============================================================================

// Send binary data (sends length as int, then raw bytes)
void send_binary(int socket, const vector<char>& data);

// Receive binary data (receives length first, then raw bytes)
vector<char> recv_binary(int socket);

// ============================================================================
// Connection establishment
// ============================================================================

// Convert IP address to string
string ip_to_string(in_addr_t address);

// Create a listening socket on the specified port
// Returns socket fd on success, -1 on failure
int listen_on_port(uint16_t listen_port);

// Accept a connection on a listening socket
// Returns new connection fd on success, -1 on failure
int accept_connection(int sockfd);

// Connect to a remote host
// address: IP address in network byte order
// Returns socket fd on success, -1 on failure
int connect_to(in_addr_t address, uint16_t remote_port);

#endif // BASE_CONNECTION_H