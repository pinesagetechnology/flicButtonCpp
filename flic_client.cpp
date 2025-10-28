#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iomanip>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include "client_protocol_packets.h"

using namespace FlicClientProtocol;

// Helper class for Bluetooth address handling
class BdAddr {
private:
    uint8_t addr[6];

    static uint8_t hexDigitToInt(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }

    static uint8_t hexToByte(const char* hex) {
        return (hexDigitToInt(hex[0]) << 4) | hexDigitToInt(hex[1]);
    }

public:
    BdAddr() {
        std::memset(addr, 0, 6);
    }

    explicit BdAddr(const std::string& addrStr) {
        fromString(addrStr);
    }

    explicit BdAddr(const uint8_t* a) {
        std::memcpy(addr, a, 6);
    }

    void fromString(const std::string& addrStr) {
        // Format: "xx:xx:xx:xx:xx:xx"
        for (int i = 0, pos = 15; i < 6; i++, pos -= 3) {
            addr[i] = hexToByte(&addrStr[pos]);
        }
    }

    std::string toString() const {
        std::stringstream ss;
        for (int i = 5; i >= 0; i--) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)addr[i];
            if (i > 0) ss << ":";
        }
        return ss.str();
    }

    const uint8_t* data() const { return addr; }
    uint8_t* data() { return addr; }
};

// Main Flic Client class
class FlicClient {
private:
    int sockfd;
    std::string host;
    int port;
    bool connected;
    
    std::unordered_map<uint32_t, std::string> connections; // conn_id -> bdaddr
    std::unordered_map<uint32_t, std::string> scanners;    // scan_id -> name

    // Helper function to write packets
    bool writePacket(const void* data, size_t len) {
        uint16_t length = static_cast<uint16_t>(len);
        
        // Write length header (little endian)
        if (write(sockfd, &length, 2) != 2) {
            std::cerr << "Failed to write packet length" << std::endl;
            return false;
        }
        
        // Write packet data
        if (write(sockfd, data, len) != static_cast<ssize_t>(len)) {
            std::cerr << "Failed to write packet data" << std::endl;
            return false;
        }
        
        return true;
    }

    // Helper function to read packets
    ssize_t readPacket(void* buffer, size_t maxLen) {
        uint16_t length;
        
        // Read length header
        ssize_t n = read(sockfd, &length, 2);
        if (n != 2) {
            if (n == 0) {
                std::cout << "Server disconnected" << std::endl;
            }
            return -1;
        }
        
        if (length > maxLen) {
            std::cerr << "Packet too large: " << length << " bytes" << std::endl;
            return -1;
        }
        
        // Read packet data
        n = read(sockfd, buffer, length);
        if (n != length) {
            std::cerr << "Failed to read complete packet" << std::endl;
            return -1;
        }
        
        return length;
    }

    void handlePacket(const uint8_t* data, size_t len) {
        if (len < 1) return;
        
        uint8_t opcode = data[0];
        
        switch (opcode) {
            case EVT_ADVERTISEMENT_PACKET_OPCODE:
                handleAdvertisementPacket(reinterpret_cast<const EvtAdvertisementPacket*>(data));
                break;
                
            case EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE:
                handleCreateConnectionChannelResponse(
                    reinterpret_cast<const EvtCreateConnectionChannelResponse*>(data));
                break;
                
            case EVT_CONNECTION_STATUS_CHANGED_OPCODE:
                handleConnectionStatusChanged(
                    reinterpret_cast<const EvtConnectionStatusChanged*>(data));
                break;
                
            case EVT_CONNECTION_CHANNEL_REMOVED_OPCODE:
                handleConnectionChannelRemoved(
                    reinterpret_cast<const EvtConnectionChannelRemoved*>(data));
                break;
                
            case EVT_BUTTON_UP_OR_DOWN_OPCODE:
                handleButtonEvent(reinterpret_cast<const EvtButtonUpOrDown*>(data));
                break;
                
            case EVT_BUTTON_CLICK_OR_HOLD_OPCODE:
                handleButtonClickOrHold(reinterpret_cast<const EvtButtonClickOrHold*>(data));
                break;
                
            case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE:
                handleButtonSingleOrDoubleClick(
                    reinterpret_cast<const EvtButtonSingleOrDoubleClick*>(data));
                break;
                
            case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE:
                handleButtonSingleOrDoubleClickOrHold(
                    reinterpret_cast<const EvtButtonSingleOrDoubleClickOrHold*>(data));
                break;
                
            case EVT_NEW_VERIFIED_BUTTON_OPCODE:
                handleNewVerifiedButton(reinterpret_cast<const EvtNewVerifiedButton*>(data));
                break;
                
            case EVT_GET_INFO_RESPONSE_OPCODE:
                handleGetInfoResponse(reinterpret_cast<const EvtGetInfoResponse*>(data), len);
                break;
                
            case EVT_NO_SPACE_FOR_NEW_CONNECTION_OPCODE:
                std::cout << "No space for new connection" << std::endl;
                break;
                
            case EVT_GOT_SPACE_FOR_NEW_CONNECTION_OPCODE:
                std::cout << "Got space for new connection" << std::endl;
                break;
                
            case EVT_BLUETOOTH_CONTROLLER_STATE_CHANGE_OPCODE:
                handleBluetoothControllerStateChange(
                    reinterpret_cast<const EvtBluetoothControllerStateChange*>(data));
                break;
                
            case EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE:
                std::cout << "Scan wizard found private button" << std::endl;
                break;
                
            case EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE:
                handleScanWizardFoundPublicButton(
                    reinterpret_cast<const EvtScanWizardFoundPublicButton*>(data));
                break;
                
            case EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE:
                std::cout << "Scan wizard: Button connected!" << std::endl;
                break;
                
            case EVT_SCAN_WIZARD_COMPLETED_OPCODE:
                handleScanWizardCompleted(reinterpret_cast<const EvtScanWizardCompleted*>(data));
                break;
                
            default:
                std::cout << "Unknown opcode: " << static_cast<int>(opcode) << std::endl;
                break;
        }
    }

    void handleAdvertisementPacket(const EvtAdvertisementPacket* evt) {
        BdAddr addr(evt->bd_addr);
        std::string name(evt->name, evt->name + evt->name_length);
        
        std::cout << "Advertisement: " << addr.toString() 
                  << " Name: " << name
                  << " RSSI: " << static_cast<int>(evt->rssi) << " dBm"
                  << " Private: " << (evt->is_private ? "yes" : "no")
                  << std::endl;
    }

    void handleCreateConnectionChannelResponse(const EvtCreateConnectionChannelResponse* evt) {
        std::cout << "Create connection channel response: ";
        switch (evt->error) {
            case NoError:
                std::cout << "Success";
                break;
            case MaxPendingConnectionsReached:
                std::cout << "Max pending connections reached";
                break;
            default:
                std::cout << "Unknown error";
                break;
        }
        std::cout << " (conn_id: " << evt->conn_id << ")" << std::endl;
    }

    void handleConnectionStatusChanged(const EvtConnectionStatusChanged* evt) {
        BdAddr addr(evt->bd_addr);
        std::cout << "Connection status changed for " << addr.toString() 
                  << " (conn_id: " << evt->conn_id << "): ";
        
        switch (evt->connection_status) {
            case Disconnected:
                std::cout << "Disconnected";
                if (evt->connection_status == Disconnected) {
                    std::cout << " - Reason: ";
                    switch (evt->disconnect_reason) {
                        case Unspecified:
                            std::cout << "Unspecified";
                            break;
                        case ConnectionEstablishmentFailed:
                            std::cout << "Connection establishment failed";
                            break;
                        case TimedOut:
                            std::cout << "Timed out";
                            break;
                        case BondingKeysMismatch:
                            std::cout << "Bonding keys mismatch";
                            break;
                        default:
                            std::cout << "Unknown";
                            break;
                    }
                }
                break;
            case Connected:
                std::cout << "Connected";
                break;
            case Ready:
                std::cout << "Ready";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << std::endl;
    }

    void handleConnectionChannelRemoved(const EvtConnectionChannelRemoved* evt) {
        std::cout << "Connection channel removed (conn_id: " << evt->conn_id << "): ";
        
        switch (evt->removed_reason) {
            case RemovedByThisClient:
                std::cout << "Removed by this client";
                break;
            case ForceDisconnectedByThisClient:
                std::cout << "Force disconnected by this client";
                break;
            case ForceDisconnectedByOtherClient:
                std::cout << "Force disconnected by other client";
                break;
            case ButtonIsPrivate:
                std::cout << "Button is private";
                break;
            case VerifyTimeout:
                std::cout << "Verify timeout";
                break;
            case InternetBackendError:
                std::cout << "Internet backend error";
                break;
            case InvalidData:
                std::cout << "Invalid data";
                break;
            case CouldntLoadDevice:
                std::cout << "Couldn't load device";
                break;
            case DeletedByThisClient:
                std::cout << "Deleted by this client";
                break;
            case DeletedByOtherClient:
                std::cout << "Deleted by other client";
                break;
            case ButtonBelongsToOtherPartner:
                std::cout << "Button belongs to other partner";
                break;
            case DeletedFromButton:
                std::cout << "Deleted from button";
                break;
            default:
                std::cout << "Unknown reason";
                break;
        }
        std::cout << std::endl;
        
        connections.erase(evt->conn_id);
    }

    void handleButtonEvent(const EvtButtonUpOrDown* evt) {
        std::cout << "Button " << (evt->click_type == ClickTypeButtonDown ? "DOWN" : "UP")
                  << " (conn_id: " << evt->conn_id 
                  << ", age: " << evt->time_diff << " ms)" << std::endl;
    }

    void handleButtonClickOrHold(const EvtButtonClickOrHold* evt) {
        std::cout << "Button " 
                  << (evt->click_type == ClickTypeButtonClick ? "CLICK" : "HOLD")
                  << " (conn_id: " << evt->conn_id 
                  << ", age: " << evt->time_diff << " ms)" << std::endl;
    }

    void handleButtonSingleOrDoubleClick(const EvtButtonSingleOrDoubleClick* evt) {
        std::cout << "Button ";
        switch (evt->click_type) {
            case ClickTypeButtonSingleClick:
                std::cout << "SINGLE CLICK";
                break;
            case ClickTypeButtonDoubleClick:
                std::cout << "DOUBLE CLICK";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << " (conn_id: " << evt->conn_id 
                  << ", age: " << evt->time_diff << " ms)" << std::endl;
    }

    void handleButtonSingleOrDoubleClickOrHold(const EvtButtonSingleOrDoubleClickOrHold* evt) {
        std::cout << "Button ";
        switch (evt->click_type) {
            case ClickTypeButtonSingleClick:
                std::cout << "SINGLE CLICK";
                break;
            case ClickTypeButtonDoubleClick:
                std::cout << "DOUBLE CLICK";
                break;
            case ClickTypeButtonHold:
                std::cout << "HOLD";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }
        std::cout << " (conn_id: " << evt->conn_id 
                  << ", age: " << evt->time_diff << " ms)" << std::endl;
    }

    void handleNewVerifiedButton(const EvtNewVerifiedButton* evt) {
        BdAddr addr(evt->bd_addr);
        std::cout << "New verified button: " << addr.toString() << std::endl;
    }

    void handleGetInfoResponse(const EvtGetInfoResponse* evt, size_t len) {
        BdAddr myAddr(evt->my_bd_addr);
        
        std::cout << "\n=== Server Info ===" << std::endl;
        std::cout << "Bluetooth controller state: ";
        switch (evt->bluetooth_controller_state) {
            case Detached:
                std::cout << "Detached";
                break;
            case Resetting:
                std::cout << "Resetting";
                break;
            case Attached:
                std::cout << "Attached";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << std::endl;
        
        std::cout << "My BD Address: " << myAddr.toString() << " (";
        switch (evt->my_bd_addr_type) {
            case PublicBdAddrType:
                std::cout << "Public";
                break;
            case RandomBdAddrType:
                std::cout << "Random";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << ")" << std::endl;
        
        std::cout << "Max pending connections: " << (int)evt->max_pending_connections << std::endl;
        std::cout << "Max concurrent connections: " << evt->max_concurrently_connected_buttons << std::endl;
        std::cout << "Current pending connections: " << (int)evt->current_pending_connection_count << std::endl;
        std::cout << "Currently no space for new connections: " 
                  << (evt->currently_no_space_for_new_connection ? "yes" : "no") << std::endl;
        
        // Parse verified buttons
        size_t offset = sizeof(EvtGetInfoResponse);
        std::cout << "\nVerified buttons:" << std::endl;
        
        uint16_t nb_verified_buttons;
        std::memcpy(&nb_verified_buttons, 
                    reinterpret_cast<const uint8_t*>(evt) + offset, 2);
        offset += 2;
        
        if (nb_verified_buttons == 0) {
            std::cout << "  (none)" << std::endl;
        } else {
            for (int i = 0; i < nb_verified_buttons; i++) {
                BdAddr buttonAddr(reinterpret_cast<const uint8_t*>(evt) + offset);
                std::cout << "  " << buttonAddr.toString() << std::endl;
                offset += 6;
            }
        }
        std::cout << "==================\n" << std::endl;
    }

    void handleBluetoothControllerStateChange(const EvtBluetoothControllerStateChange* evt) {
        std::cout << "Bluetooth controller state changed to: ";
        switch (evt->state) {
            case Detached:
                std::cout << "Detached";
                break;
            case Resetting:
                std::cout << "Resetting";
                break;
            case Attached:
                std::cout << "Attached";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << std::endl;
    }

    void handleScanWizardFoundPublicButton(const EvtScanWizardFoundPublicButton* evt) {
        BdAddr addr(evt->bd_addr);
        std::string name(evt->name, evt->name + evt->name_length);
        
        std::cout << "Scan wizard found button: " << addr.toString() 
                  << " Name: " << name << std::endl;
    }

    void handleScanWizardCompleted(const EvtScanWizardCompleted* evt) {
        std::cout << "Scan wizard completed: ";
        
        switch (evt->result) {
            case WizardSuccess:
                std::cout << "Success!" << std::endl;
                break;
            case WizardCancelledByUser:
                std::cout << "Cancelled by user" << std::endl;
                break;
            case WizardFailedTimeout:
                std::cout << "Failed (timeout)" << std::endl;
                break;
            case WizardButtonIsPrivate:
                std::cout << "Button is private" << std::endl;
                break;
            case WizardBluetoothUnavailable:
                std::cout << "Bluetooth unavailable" << std::endl;
                break;
            case WizardInternetBackendError:
                std::cout << "Internet backend error" << std::endl;
                break;
            case WizardInvalidData:
                std::cout << "Invalid data" << std::endl;
                break;
            case WizardButtonBelongsToOtherPartner:
                std::cout << "Button belongs to other partner" << std::endl;
                break;
            case WizardButtonAlreadyConnectedToOtherDevice:
                std::cout << "Button already connected to other device" << std::endl;
                break;
            default:
                std::cout << "Unknown result" << std::endl;
                break;
        }
    }

    void printHelp() {
        std::cout << "\n=== Available Commands ===" << std::endl;
        std::cout << "getInfo                                  - Get server info" << std::endl;
        std::cout << "startScanWizard                          - Start scan wizard (pair new button)" << std::endl;
        std::cout << "cancelScanWizard                         - Cancel scan wizard" << std::endl;
        std::cout << "startScan                                - Start raw button scanning" << std::endl;
        std::cout << "stopScan                                 - Stop raw button scanning" << std::endl;
        std::cout << "connect <bdaddr> <conn_id>               - Connect to button" << std::endl;
        std::cout << "disconnect <conn_id>                     - Disconnect button" << std::endl;
        std::cout << "forceDisconnect <bdaddr>                 - Force disconnect button" << std::endl;
        std::cout << "getButtonInfo <bdaddr>                   - Get button info" << std::endl;
        std::cout << "deleteButton <bdaddr>                    - Delete button pairing" << std::endl;
        std::cout << "help                                     - Show this help" << std::endl;
        std::cout << "quit                                     - Exit client" << std::endl;
        std::cout << "==========================\n" << std::endl;
    }

public:
    FlicClient(const std::string& host, int port = 5551)
        : sockfd(-1), host(host), port(port), connected(false) {}

    ~FlicClient() {
        disconnect();
    }

    bool connect() {
        // Create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Resolve hostname
        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            std::cerr << "Failed to resolve host: " << host << std::endl;
            close(sockfd);
            sockfd = -1;
            return false;
        }

        // Setup server address
        struct sockaddr_in serv_addr;
        std::memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        // Connect
        if (::connect(sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), 
                      sizeof(serv_addr)) < 0) {
            std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
            close(sockfd);
            sockfd = -1;
            return false;
        }

        connected = true;
        std::cout << "Connected to Flic server at " << host << ":" << port << std::endl;
        
        // Immediately request server info
        getInfo();
        
        return true;
    }

    void disconnect() {
        if (sockfd >= 0) {
            close(sockfd);
            sockfd = -1;
        }
        connected = false;
    }

    void getInfo() {
        CmdGetInfo cmd;
        cmd.opcode = CMD_GET_INFO_OPCODE;
        writePacket(&cmd, sizeof(cmd));
    }

    void startScanWizard(uint32_t scan_wizard_id = 0) {
        CmdCreateScanWizard cmd;
        cmd.opcode = CMD_CREATE_SCAN_WIZARD_OPCODE;
        cmd.scan_wizard_id = scan_wizard_id;
        writePacket(&cmd, sizeof(cmd));
        std::cout << "Scan wizard started. Press and hold your Flic button..." << std::endl;
    }

    void cancelScanWizard(uint32_t scan_wizard_id = 0) {
        CmdCancelScanWizard cmd;
        cmd.opcode = CMD_CANCEL_SCAN_WIZARD_OPCODE;
        cmd.scan_wizard_id = scan_wizard_id;
        writePacket(&cmd, sizeof(cmd));
    }

    void startScan(uint32_t scan_id = 0) {
        CmdCreateScanner cmd;
        cmd.opcode = CMD_CREATE_SCANNER_OPCODE;
        cmd.scan_id = scan_id;
        writePacket(&cmd, sizeof(cmd));
        scanners[scan_id] = "scanner";
        std::cout << "Started scanning..." << std::endl;
    }

    void stopScan(uint32_t scan_id = 0) {
        CmdRemoveScanner cmd;
        cmd.opcode = CMD_REMOVE_SCANNER_OPCODE;
        cmd.scan_id = scan_id;
        writePacket(&cmd, sizeof(cmd));
        scanners.erase(scan_id);
        std::cout << "Stopped scanning" << std::endl;
    }

    void connectButton(const std::string& bdaddr, uint32_t conn_id) {
        CmdCreateConnectionChannel cmd;
        cmd.opcode = CMD_CREATE_CONNECTION_CHANNEL_OPCODE;
        
        BdAddr addr(bdaddr);
        std::memcpy(cmd.bd_addr, addr.data(), 6);
        
        cmd.conn_id = conn_id;
        cmd.latency_mode = NormalLatency;
        cmd.auto_disconnect_time = 0x1ff;
        
        writePacket(&cmd, sizeof(cmd));
        connections[conn_id] = bdaddr;
        std::cout << "Connecting to " << bdaddr << "..." << std::endl;
    }

    void disconnectButton(uint32_t conn_id) {
        CmdRemoveConnectionChannel cmd;
        cmd.opcode = CMD_REMOVE_CONNECTION_CHANNEL_OPCODE;
        cmd.conn_id = conn_id;
        writePacket(&cmd, sizeof(cmd));
    }

    void forceDisconnect(const std::string& bdaddr) {
        CmdForceDisconnect cmd;
        cmd.opcode = CMD_FORCE_DISCONNECT_OPCODE;
        
        BdAddr addr(bdaddr);
        std::memcpy(cmd.bd_addr, addr.data(), 6);
        
        writePacket(&cmd, sizeof(cmd));
        std::cout << "Force disconnecting " << bdaddr << std::endl;
    }

    void getButtonInfo(const std::string& bdaddr) {
        CmdGetButtonInfo cmd;
        cmd.opcode = CMD_GET_BUTTON_INFO_OPCODE;
        
        BdAddr addr(bdaddr);
        std::memcpy(cmd.bd_addr, addr.data(), 6);
        
        writePacket(&cmd, sizeof(cmd));
    }

    void deleteButton(const std::string& bdaddr) {
        CmdDeleteButton cmd;
        cmd.opcode = CMD_DELETE_BUTTON_OPCODE;
        
        BdAddr addr(bdaddr);
        std::memcpy(cmd.bd_addr, addr.data(), 6);
        
        writePacket(&cmd, sizeof(cmd));
        std::cout << "Deleting button " << bdaddr << std::endl;
    }

    void run() {
        if (!connected) {
            std::cerr << "Not connected" << std::endl;
            return;
        }

        printHelp();

        // Main loop with poll
        struct pollfd fds[2];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        fds[1].fd = STDIN_FILENO;
        fds[1].events = POLLIN;

        uint8_t buffer[1024];
        std::string inputLine;

        while (connected) {
            int ret = poll(fds, 2, -1);
            
            if (ret < 0) {
                perror("poll");
                break;
            }

            // Handle server messages
            if (fds[0].revents & POLLIN) {
                ssize_t len = readPacket(buffer, sizeof(buffer));
                if (len < 0) {
                    break;
                }
                handlePacket(buffer, len);
            }

            // Handle user input
            if (fds[1].revents & POLLIN) {
                std::string line;
                if (!std::getline(std::cin, line)) {
                    break;
                }

                std::istringstream iss(line);
                std::string cmd;
                iss >> cmd;

                if (cmd == "quit" || cmd == "exit") {
                    break;
                } else if (cmd == "help") {
                    printHelp();
                } else if (cmd == "getInfo") {
                    getInfo();
                } else if (cmd == "startScanWizard") {
                    startScanWizard();
                } else if (cmd == "cancelScanWizard") {
                    cancelScanWizard();
                } else if (cmd == "startScan") {
                    startScan();
                } else if (cmd == "stopScan") {
                    stopScan();
                } else if (cmd == "connect") {
                    std::string bdaddr;
                    uint32_t conn_id;
                    if (iss >> bdaddr >> conn_id) {
                        connectButton(bdaddr, conn_id);
                    } else {
                        std::cout << "Usage: connect <bdaddr> <conn_id>" << std::endl;
                    }
                } else if (cmd == "disconnect") {
                    uint32_t conn_id;
                    if (iss >> conn_id) {
                        disconnectButton(conn_id);
                    } else {
                        std::cout << "Usage: disconnect <conn_id>" << std::endl;
                    }
                } else if (cmd == "forceDisconnect") {
                    std::string bdaddr;
                    if (iss >> bdaddr) {
                        forceDisconnect(bdaddr);
                    } else {
                        std::cout << "Usage: forceDisconnect <bdaddr>" << std::endl;
                    }
                } else if (cmd == "getButtonInfo") {
                    std::string bdaddr;
                    if (iss >> bdaddr) {
                        getButtonInfo(bdaddr);
                    } else {
                        std::cout << "Usage: getButtonInfo <bdaddr>" << std::endl;
                    }
                } else if (cmd == "deleteButton") {
                    std::string bdaddr;
                    if (iss >> bdaddr) {
                        deleteButton(bdaddr);
                    } else {
                        std::cout << "Usage: deleteButton <bdaddr>" << std::endl;
                    }
                } else if (!cmd.empty()) {
                    std::cout << "Unknown command: " << cmd << std::endl;
                    std::cout << "Type 'help' for available commands" << std::endl;
                }
            }
        }

        std::cout << "Disconnecting..." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <host> [port]" << std::endl;
        std::cerr << "Example: " << argv[0] << " localhost 5551" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    int port = (argc >= 3) ? std::atoi(argv[2]) : 5551;

    FlicClient client(host, port);
    
    if (!client.connect()) {
        return 1;
    }

    client.run();

    return 0;
}