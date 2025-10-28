#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <iostream>
#include <map>
#include <string>
#include <ctime>
#include <stdexcept>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    #define ioctl ioctlsocket
    #define FIONREAD FIONREAD
#else
    #include <unistd.h>
    #include <poll.h>
    #include <sys/param.h>
    #include <sys/uio.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif

#include "fliclib-linux-hci/simpleclient/client_protocol_packets.h"

using namespace std;
using namespace FlicClientProtocol;

class FlicCustomClient {
private:
#ifdef _WIN32
    SOCKET sockfd;
#else
    int sockfd;
#endif
    bool connected;
    map<uint32_t, string> connection_channels; // conn_id -> button_address
    uint32_t next_conn_id;
    uint32_t scan_wizard_id;

    // Helper function to convert bytes to hex string
    string bytes_to_hex_string(const uint8_t* data, int len) {
        string str(len * 2, '\0');
        for (int i = 0; i < len; i++) {
            static const char tbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            str[i * 2] = tbl[data[i] >> 4];
            str[i * 2 + 1] = tbl[data[i] & 0xf];
        }
        return str;
    }

    // Helper function to convert hex to byte
    uint8_t hex_to_byte(const char* hex) {
        auto hex_digit_to_int = [](char hex) -> uint8_t {
            if (hex >= '0' && hex <= '9') return hex - '0';
            if (hex >= 'a' && hex <= 'f') return hex - 'a' + 10;
            if (hex >= 'A' && hex <= 'F') return hex - 'A' + 10;
            return 0;
        };
        return (hex_digit_to_int(hex[0]) << 4) | hex_digit_to_int(hex[1]);
    }

    // Helper function to parse Bluetooth address
    void parse_bdaddr(const string& addr_str, uint8_t* bd_addr) {
        if (addr_str.length() != 17) {
            throw invalid_argument("Invalid Bluetooth address format");
        }
        
        for (int i = 0, pos = 15; i < 6; i++, pos -= 3) {
            bd_addr[i] = hex_to_byte(&addr_str[pos]);
        }
    }

    // Helper function to format Bluetooth address
    string format_bdaddr(const uint8_t* addr) {
        string str;
        for (int i = 5; i >= 0; i--) {
            str += bytes_to_hex_string(addr + i, 1);
            if (i != 0) {
                str += ':';
            }
        }
        return str;
    }

    // Write packet to socket
    void write_packet(void* buf, int len) {
        uint8_t new_buf[2 + len];
        new_buf[0] = len & 0xff;
        new_buf[1] = len >> 8;
        memcpy(new_buf + 2, buf, len);
        
        int pos = 0;
        int left = 2 + len;
        while(left) {
            int res = write(sockfd, new_buf + pos, left);
            if (res < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("write");
                throw runtime_error("Failed to write packet");
            }
            pos += res;
            left -= res;
        }
    }

    // Read packet from socket
    int read_packet(uint8_t* buf, int max_len) {
        int bytes_available;
        if (ioctl(sockfd, FIONREAD, &bytes_available) < 0) {
            perror("ioctl");
            return -1;
        }
        
        if (bytes_available == 0) {
            return 0; // Connection closed
        }
        
        if (bytes_available < 2) {
            return -2; // Not enough data for header
        }
        
        // Read length header
        int nbytes = read(sockfd, buf, 2);
        if (nbytes < 0) {
            perror("read header");
            return -1;
        }
        
        int packet_len = buf[0] | (buf[1] << 8);
        if (packet_len > max_len - 2) {
            cerr << "Packet too large: " << packet_len << endl;
            return -1;
        }
        
        // Read packet data
        int read_pos = 0;
        int bytes_left = packet_len;
        
        while (bytes_left > 0) {
            nbytes = read(sockfd, buf + 2 + read_pos, bytes_left);
            if (nbytes < 0) {
                perror("read data");
                return -1;
            }
            read_pos += nbytes;
            bytes_left -= nbytes;
        }
        
        return packet_len + 2;
    }

    // Handle incoming events
    void handle_event(uint8_t* buf, int len) {
        if (len < 1) return;
        
        uint8_t opcode = buf[0];
        
        switch (opcode) {
            case EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE: {
                cout << "[PAIRING] Found private button. Please hold it down for 7 seconds to make it public." << endl;
                break;
            }
            case EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE: {
                EvtScanWizardFoundPublicButton* evt = (EvtScanWizardFoundPublicButton*)buf;
                string button_addr = format_bdaddr(evt->bd_addr);
                string button_name = string(evt->name, evt->name_length);
                cout << "[PAIRING] Found public button " << button_addr << " (" << button_name << "), connecting..." << endl;
                break;
            }
            case EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE: {
                cout << "[PAIRING] Connected, now pairing and verifying..." << endl;
                break;
            }
            case EVT_SCAN_WIZARD_COMPLETED_OPCODE: {
                EvtScanWizardCompleted* evt = (EvtScanWizardCompleted*)buf;
                static const char* results[] = {
                    "WizardSuccess", "WizardCancelledByUser", "WizardFailedTimeout",
                    "WizardButtonIsPrivate", "WizardBluetoothUnavailable", "WizardInternetBackendError",
                    "WizardInvalidData", "WizardButtonBelongsToOtherPartner", "WizardButtonAlreadyConnectedToOtherDevice"
                };
                cout << "[PAIRING] Scan wizard completed with status: " << results[evt->result] << endl;
                if (evt->result == WizardSuccess) {
                    cout << "[PAIRING] Button successfully paired!" << endl;
                }
                break;
            }
            case EVT_NEW_VERIFIED_BUTTON_OPCODE: {
                EvtNewVerifiedButton* evt = (EvtNewVerifiedButton*)buf;
                string button_addr = format_bdaddr(evt->bd_addr);
                cout << "[INFO] New verified button: " << button_addr << endl;
                break;
            }
            case EVT_GET_INFO_RESPONSE_OPCODE: {
                EvtGetInfoResponse* evt = (EvtGetInfoResponse*)buf;
                cout << "[INFO] Server Info:" << endl;
                cout << "  Bluetooth Controller: " << (evt->bluetooth_controller_state == Attached ? "Attached" : "Detached") << endl;
                cout << "  My Address: " << format_bdaddr(evt->my_bd_addr) << endl;
                cout << "  Max Connections: " << (int)evt->max_concurrently_connected_buttons << endl;
                cout << "  Verified Buttons (" << evt->nb_verified_buttons << "):" << endl;
                
                for(int i = 0; i < evt->nb_verified_buttons; i++) {
                    string button_addr = format_bdaddr(evt->bd_addr_of_verified_buttons[i]);
                    cout << "    " << button_addr << endl;
                }
                break;
            }
            case EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE: {
                EvtCreateConnectionChannelResponse* evt = (EvtCreateConnectionChannelResponse*)buf;
                static const char* errors[] = {"NoError", "MaxPendingConnectionsReached"};
                static const char* statuses[] = {"Disconnected", "Connected", "Ready"};
                cout << "[CONNECTION] Channel " << evt->base.conn_id << ": " 
                     << errors[evt->error] << ", Status: " << statuses[evt->connection_status] << endl;
                break;
            }
            case EVT_CONNECTION_STATUS_CHANGED_OPCODE: {
                EvtConnectionStatusChanged* evt = (EvtConnectionStatusChanged*)buf;
                static const char* statuses[] = {"Disconnected", "Connected", "Ready"};
                cout << "[CONNECTION] Channel " << evt->base.conn_id << " status changed to: " 
                     << statuses[evt->connection_status] << endl;
                break;
            }
            case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE: {
                EvtButtonEvent* evt = (EvtButtonEvent*)buf;
                static const char* click_types[] = {
                    "ButtonDown", "ButtonUp", "ButtonClick", 
                    "ButtonSingleClick", "ButtonDoubleClick", "ButtonHold"
                };
                
                // Get current time for logging
                time_t now = time(0);
                char* time_str = ctime(&now);
                time_str[strlen(time_str)-1] = '\0'; // Remove newline
                
                cout << "[" << time_str << "] BUTTON ACTION: " 
                     << click_types[evt->click_type] 
                     << " (Channel: " << evt->base.conn_id 
                     << ", Queued: " << (evt->was_queued ? "Yes" : "No") 
                     << ", Time diff: " << evt->time_diff << "s)" << endl;
                break;
            }
            case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE: {
                EvtButtonEvent* evt = (EvtButtonEvent*)buf;
                static const char* click_types[] = {
                    "ButtonDown", "ButtonUp", "ButtonClick", 
                    "ButtonSingleClick", "ButtonDoubleClick", "ButtonHold"
                };
                
                time_t now = time(0);
                char* time_str = ctime(&now);
                time_str[strlen(time_str)-1] = '\0';
                
                cout << "[" << time_str << "] BUTTON ACTION: " 
                     << click_types[evt->click_type] 
                     << " (Channel: " << evt->base.conn_id 
                     << ", Queued: " << (evt->was_queued ? "Yes" : "No") 
                     << ", Time diff: " << evt->time_diff << "s)" << endl;
                break;
            }
            case EVT_BUTTON_CLICK_OR_HOLD_OPCODE: {
                EvtButtonEvent* evt = (EvtButtonEvent*)buf;
                static const char* click_types[] = {
                    "ButtonDown", "ButtonUp", "ButtonClick", 
                    "ButtonSingleClick", "ButtonDoubleClick", "ButtonHold"
                };
                
                time_t now = time(0);
                char* time_str = ctime(&now);
                time_str[strlen(time_str)-1] = '\0';
                
                cout << "[" << time_str << "] BUTTON ACTION: " 
                     << click_types[evt->click_type] 
                     << " (Channel: " << evt->base.conn_id 
                     << ", Queued: " << (evt->was_queued ? "Yes" : "No") 
                     << ", Time diff: " << evt->time_diff << "s)" << endl;
                break;
            }
            case EVT_BUTTON_UP_OR_DOWN_OPCODE: {
                EvtButtonEvent* evt = (EvtButtonEvent*)buf;
                static const char* click_types[] = {
                    "ButtonDown", "ButtonUp", "ButtonClick", 
                    "ButtonSingleClick", "ButtonDoubleClick", "ButtonHold"
                };
                
                time_t now = time(0);
                char* time_str = ctime(&now);
                time_str[strlen(time_str)-1] = '\0';
                
                cout << "[" << time_str << "] BUTTON ACTION: " 
                     << click_types[evt->click_type] 
                     << " (Channel: " << evt->base.conn_id 
                     << ", Queued: " << (evt->was_queued ? "Yes" : "No") 
                     << ", Time diff: " << evt->time_diff << "s)" << endl;
                break;
            }
            default:
                // Ignore other events
                break;
        }
    }

public:
    FlicCustomClient() : connected(false), next_conn_id(1), scan_wizard_id(1) {
#ifdef _WIN32
        sockfd = INVALID_SOCKET;
#else
        sockfd = -1;
#endif
    }

    ~FlicCustomClient() {
#ifdef _WIN32
        if (sockfd != INVALID_SOCKET) {
            closesocket(sockfd);
        }
        WSACleanup();
#else
        if (sockfd >= 0) {
            close(sockfd);
        }
#endif
    }

    // Connect to Flic daemon
    bool connect(const string& host = "localhost", int port = 5551) {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            cerr << "WSAStartup failed: " << result << endl;
            return false;
        }
        
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET) {
            cerr << "socket failed: " << WSAGetLastError() << endl;
            WSACleanup();
            return false;
        }
#else
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return false;
        }
#endif

        struct hostent* server = gethostbyname(host.c_str());
        if (server == NULL) {
            cerr << "ERROR: No such host: " << host << endl;
#ifdef _WIN32
            closesocket(sockfd);
            WSACleanup();
#else
            close(sockfd);
#endif
            return false;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port);

#ifdef _WIN32
        if (::connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
            cerr << "connect failed: " << WSAGetLastError() << endl;
            closesocket(sockfd);
            WSACleanup();
            return false;
        }
#else
        if (::connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");
            close(sockfd);
            return false;
        }
#endif

        connected = true;
        cout << "[CONNECT] Connected to Flic daemon at " << host << ":" << port << endl;
        return true;
    }

    // Start pairing process
    void start_pairing() {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        CmdCreateScanWizard cmd;
        cmd.opcode = CMD_CREATE_SCAN_WIZARD_OPCODE;
        cmd.scan_wizard_id = scan_wizard_id++;
        
        write_packet(&cmd, sizeof(cmd));
        cout << "[PAIRING] Starting scan wizard. Please click and hold your Flic button!" << endl;
    }

    // Cancel pairing process
    void cancel_pairing() {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        CmdCancelScanWizard cmd;
        cmd.opcode = CMD_CANCEL_SCAN_WIZARD_OPCODE;
        cmd.scan_wizard_id = scan_wizard_id - 1; // Use the last created wizard
        
        write_packet(&cmd, sizeof(cmd));
        cout << "[PAIRING] Scan wizard cancelled" << endl;
    }

    // Get server info and list paired buttons
    void get_info() {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        CmdGetInfo cmd;
        cmd.opcode = CMD_GET_INFO_OPCODE;
        write_packet(&cmd, sizeof(cmd));
    }

    // Connect to a specific button
    void connect_button(const string& button_addr) {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        uint8_t bd_addr[6];
        try {
            parse_bdaddr(button_addr, bd_addr);
        } catch (const invalid_argument& e) {
            cerr << "Invalid button address: " << e.what() << endl;
            return;
        }

        CmdCreateConnectionChannel cmd;
        cmd.opcode = CMD_CREATE_CONNECTION_CHANNEL_OPCODE;
        cmd.conn_id = next_conn_id++;
        memcpy(cmd.bd_addr, bd_addr, 6);
        cmd.latency_mode = NormalLatency;
        cmd.auto_disconnect_time = 0x1ff; // Keep connected

        connection_channels[cmd.conn_id] = button_addr;
        write_packet(&cmd, sizeof(cmd));
        
        cout << "[CONNECTION] Connecting to button " << button_addr << " (Channel: " << cmd.conn_id << ")" << endl;
    }

    // Disconnect from a button
    void disconnect_button(uint32_t conn_id) {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        CmdRemoveConnectionChannel cmd;
        cmd.opcode = CMD_REMOVE_CONNECTION_CHANNEL_OPCODE;
        cmd.conn_id = conn_id;
        
        write_packet(&cmd, sizeof(cmd));
        
        auto it = connection_channels.find(conn_id);
        if (it != connection_channels.end()) {
            cout << "[CONNECTION] Disconnecting from button " << it->second << " (Channel: " << conn_id << ")" << endl;
            connection_channels.erase(it);
        }
    }

    // Main event loop
    void run_event_loop() {
        if (!connected) {
            cerr << "Not connected to daemon" << endl;
            return;
        }

        cout << "[EVENT LOOP] Starting event loop. Press Ctrl+C to exit." << endl;
        cout << "[EVENT LOOP] Listening for button events..." << endl;

        uint8_t readbuf[65537];
        
        while (connected) {
#ifdef _WIN32
            fd_set fdread;
            FD_ZERO(&fdread);
            FD_SET(sockfd, &fdread);
            
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int select_res = select(0, &fdread, NULL, NULL, &timeout);
            
            if (select_res == SOCKET_ERROR) {
                cerr << "select failed: " << WSAGetLastError() << endl;
                break;
            }
#else
            fd_set fdread;
            FD_ZERO(&fdread);
            FD_SET(sockfd, &fdread);
            
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int select_res = select(sockfd + 1, &fdread, NULL, NULL, &timeout);
            
            if (select_res < 0) {
                if (errno == EINTR) {
                    continue;
                } else {
                    perror("select");
                    break;
                }
            }
#endif
            
            if (select_res == 0) {
                // Timeout - continue loop
                continue;
            }
            
            if (FD_ISSET(sockfd, &fdread)) {
                int len = read_packet(readbuf, sizeof(readbuf));
                if (len == 0) {
                    cout << "[EVENT LOOP] Server closed connection" << endl;
                    break;
                } else if (len < 0) {
                    if (len == -2) continue; // Not enough data
                    break; // Error
                }
                
                handle_event(readbuf, len);
            }
        }
    }

    // Print help
    void print_help() {
        cout << "\n=== Flic Custom Client Commands ===" << endl;
        cout << "1. pair          - Start pairing process" << endl;
        cout << "2. cancel        - Cancel pairing process" << endl;
        cout << "3. list          - List paired buttons" << endl;
        cout << "4. connect <addr> - Connect to specific button (e.g., connect aa:bb:cc:dd:ee:ff)" << endl;
        cout << "5. disconnect <id> - Disconnect from button by channel ID" << endl;
        cout << "6. listen        - Start listening for button events" << endl;
        cout << "7. help          - Show this help" << endl;
        cout << "8. quit          - Exit application" << endl;
        cout << "\nNote: Button events will be logged as:" << endl;
        cout << "  - ButtonSingleClick: Single press" << endl;
        cout << "  - ButtonDoubleClick: Double press" << endl;
        cout << "  - ButtonHold: Long press (hold)" << endl;
        cout << "=====================================\n" << endl;
    }
};

int main(int argc, char* argv[]) {
    cout << "=== Flic Custom Client ===" << endl;
    cout << "Connecting to Flic daemon..." << endl;

    FlicCustomClient client;
    
    // Connect to daemon
    string host = "localhost";
    int port = 5551;
    
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    if (!client.connect(host, port)) {
        cerr << "Failed to connect to Flic daemon at " << host << ":" << port << endl;
        cerr << "Make sure the Flic daemon (flicd) is running!" << endl;
        return 1;
    }

    // Get initial info
    client.get_info();
    
    // Interactive command loop
    string command;
    client.print_help();
    
    while (true) {
        cout << "flic> ";
        cin >> command;
        
        if (command == "pair") {
            client.start_pairing();
        } else if (command == "cancel") {
            client.cancel_pairing();
        } else if (command == "list") {
            client.get_info();
        } else if (command == "connect") {
            string addr;
            cout << "Enter button address (e.g., aa:bb:cc:dd:ee:ff): ";
            cin >> addr;
            client.connect_button(addr);
        } else if (command == "disconnect") {
            uint32_t conn_id;
            cout << "Enter connection ID: ";
            cin >> conn_id;
            client.disconnect_button(conn_id);
        } else if (command == "listen") {
            client.run_event_loop();
        } else if (command == "help") {
            client.print_help();
        } else if (command == "quit" || command == "exit") {
            break;
        } else {
            cout << "Unknown command: " << command << ". Type 'help' for available commands." << endl;
        }
    }
    
    cout << "Goodbye!" << endl;
    return 0;
}
