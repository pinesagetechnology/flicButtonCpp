/**
 * Flic Protocol Packets
 * Protocol specification: https://github.com/50ButtonsEach/fliclib-linux-hci/blob/master/ProtocolDocumentation.md
 * Released under CC0 (public domain)
 */

 #ifndef CLIENT_PROTOCOL_PACKETS_H
 #define CLIENT_PROTOCOL_PACKETS_H
 
 #include <stdint.h>
 
 #ifdef __cplusplus
 namespace FlicClientProtocol {
 #endif
 
 // Type definitions
 typedef uint8_t bdaddr_t[6];
 
 // Enums
 enum CreateConnectionChannelError {
     NoError,
     MaxPendingConnectionsReached
 };
 
 enum ConnectionStatus {
     Disconnected,
     Connected,
     Ready
 };
 
 enum DisconnectReason {
     Unspecified,
     ConnectionEstablishmentFailed,
     TimedOut,
     BondingKeysMismatch
 };
 
 enum RemovedReason {
     RemovedByThisClient,
     ForceDisconnectedByThisClient,
     ForceDisconnectedByOtherClient,
     
     ButtonIsPrivate,
     VerifyTimeout,
     InternetBackendError,
     InvalidData,
     
     CouldntLoadDevice,
     
     DeletedByThisClient,
     DeletedByOtherClient,
     
     ButtonBelongsToOtherPartner,
     DeletedFromButton
 };
 
 enum ClickType {
     ClickTypeButtonDown,
     ClickTypeButtonUp,
     ClickTypeButtonClick,
     ClickTypeButtonSingleClick,
     ClickTypeButtonDoubleClick,
     ClickTypeButtonHold
 };
 
 enum BdAddrType {
     PublicBdAddrType,
     RandomBdAddrType
 };
 
 enum LatencyMode {
     NormalLatency,
     LowLatency,
     HighLatency
 };
 
 enum BluetoothControllerState {
     Detached,
     Resetting,
     Attached
 };
 
 enum ScanWizardResult {
     WizardSuccess,
     WizardCancelledByUser,
     WizardFailedTimeout,
     WizardButtonIsPrivate,
     WizardBluetoothUnavailable,
     WizardInternetBackendError,
     WizardInvalidData,
     WizardButtonBelongsToOtherPartner,
     WizardButtonAlreadyConnectedToOtherDevice
 };
 
 enum BatteryStatus {
     BatteryStatusOk = 0,
     BatteryStatusLow = 1,
     BatteryStatusCritical = 2
 };
 
 // Command opcodes
 enum CommandOpcode {
     CMD_GET_INFO_OPCODE = 0,
     CMD_CREATE_SCANNER_OPCODE = 1,
     CMD_REMOVE_SCANNER_OPCODE = 2,
     CMD_CREATE_CONNECTION_CHANNEL_OPCODE = 3,
     CMD_REMOVE_CONNECTION_CHANNEL_OPCODE = 4,
     CMD_FORCE_DISCONNECT_OPCODE = 5,
     CMD_CHANGE_MODE_PARAMETERS_OPCODE = 6,
     CMD_PING_OPCODE = 7,
     CMD_GET_BUTTON_INFO_OPCODE = 8,
     CMD_CREATE_SCAN_WIZARD_OPCODE = 9,
     CMD_CANCEL_SCAN_WIZARD_OPCODE = 10,
     CMD_DELETE_BUTTON_OPCODE = 11,
     CMD_CREATE_BATTERY_STATUS_LISTENER_OPCODE = 12,
     CMD_REMOVE_BATTERY_STATUS_LISTENER_OPCODE = 13
 };
 
 // Event opcodes
 enum EventOpcode {
     EVT_ADVERTISEMENT_PACKET_OPCODE = 0,
     EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE = 1,
     EVT_CONNECTION_STATUS_CHANGED_OPCODE = 2,
     EVT_CONNECTION_CHANNEL_REMOVED_OPCODE = 3,
     EVT_BUTTON_UP_OR_DOWN_OPCODE = 4,
     EVT_BUTTON_CLICK_OR_HOLD_OPCODE = 5,
     EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE = 6,
     EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE = 7,
     EVT_NEW_VERIFIED_BUTTON_OPCODE = 8,
     EVT_GET_INFO_RESPONSE_OPCODE = 9,
     EVT_NO_SPACE_FOR_NEW_CONNECTION_OPCODE = 10,
     EVT_GOT_SPACE_FOR_NEW_CONNECTION_OPCODE = 11,
     EVT_BLUETOOTH_CONTROLLER_STATE_CHANGE_OPCODE = 12,
     EVT_PING_RESPONSE_OPCODE = 13,
     EVT_GET_BUTTON_INFO_RESPONSE_OPCODE = 14,
     EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE = 15,
     EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE = 16,
     EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE = 17,
     EVT_SCAN_WIZARD_COMPLETED_OPCODE = 18,
     EVT_BUTTON_DELETED_OPCODE = 19,
     EVT_BATTERY_STATUS_OPCODE = 20
 };
 
 // Command packets
 #pragma pack(push, 1)
 
 struct CmdGetInfo {
     uint8_t opcode;
 };
 
 struct CmdCreateScanner {
     uint8_t opcode;
     uint32_t scan_id;
 };
 
 struct CmdRemoveScanner {
     uint8_t opcode;
     uint32_t scan_id;
 };
 
 struct CmdCreateConnectionChannel {
     uint8_t opcode;
     uint32_t conn_id;
     bdaddr_t bd_addr;
     uint8_t latency_mode;
     int16_t auto_disconnect_time;
 };
 
 struct CmdRemoveConnectionChannel {
     uint8_t opcode;
     uint32_t conn_id;
 };
 
 struct CmdForceDisconnect {
     uint8_t opcode;
     bdaddr_t bd_addr;
 };
 
 struct CmdChangeModeParameters {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t latency_mode;
     int16_t auto_disconnect_time;
 };
 
 struct CmdPing {
     uint8_t opcode;
     uint32_t ping_id;
 };
 
 struct CmdGetButtonInfo {
     uint8_t opcode;
     bdaddr_t bd_addr;
 };
 
 struct CmdCreateScanWizard {
     uint8_t opcode;
     uint32_t scan_wizard_id;
 };
 
 struct CmdCancelScanWizard {
     uint8_t opcode;
     uint32_t scan_wizard_id;
 };
 
 struct CmdDeleteButton {
     uint8_t opcode;
     bdaddr_t bd_addr;
 };
 
 struct CmdCreateBatteryStatusListener {
     uint8_t opcode;
     uint32_t listener_id;
     bdaddr_t bd_addr;
 };
 
 struct CmdRemoveBatteryStatusListener {
     uint8_t opcode;
     uint32_t listener_id;
 };
 
 // Event packets
 
 struct EvtAdvertisementPacket {
     uint8_t opcode;
     uint32_t scan_id;
     bdaddr_t bd_addr;
     uint8_t name_length;
     char name[16];
     int8_t rssi;
     uint8_t is_private;
     uint8_t already_verified;
     uint8_t already_connected_to_this_device;
     uint8_t already_connected_to_other_device;
 };
 
 struct EvtCreateConnectionChannelResponse {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t error;
     uint8_t connection_status;
 };
 
 struct EvtConnectionStatusChanged {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t connection_status;
     uint8_t disconnect_reason;
     bdaddr_t bd_addr;
 };
 
 struct EvtConnectionChannelRemoved {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t removed_reason;
 };
 
 struct EvtButtonUpOrDown {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t click_type;
     uint8_t was_queued;
     uint32_t time_diff;
 };
 
 struct EvtButtonClickOrHold {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t click_type;
     uint8_t was_queued;
     uint32_t time_diff;
 };
 
 struct EvtButtonSingleOrDoubleClick {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t click_type;
     uint8_t was_queued;
     uint32_t time_diff;
 };
 
 struct EvtButtonSingleOrDoubleClickOrHold {
     uint8_t opcode;
     uint32_t conn_id;
     uint8_t click_type;
     uint8_t was_queued;
     uint32_t time_diff;
 };
 
 struct EvtNewVerifiedButton {
     uint8_t opcode;
     bdaddr_t bd_addr;
 };
 
 struct EvtGetInfoResponse {
     uint8_t opcode;
     uint8_t bluetooth_controller_state;
     bdaddr_t my_bd_addr;
     uint8_t my_bd_addr_type;
     uint8_t max_pending_connections;
     int16_t max_concurrently_connected_buttons;
     uint8_t current_pending_connection_count;
     uint8_t currently_no_space_for_new_connection;
     // Followed by:
     // uint16_t nb_verified_buttons;
     // bdaddr_t bd_addr_of_verified_buttons[nb_verified_buttons];
 };
 
 struct EvtNoSpaceForNewConnection {
     uint8_t opcode;
     uint8_t max_concurrently_connected_buttons;
 };
 
 struct EvtGotSpaceForNewConnection {
     uint8_t opcode;
     uint8_t max_concurrently_connected_buttons;
 };
 
 struct EvtBluetoothControllerStateChange {
     uint8_t opcode;
     uint8_t state;
 };
 
 struct EvtPingResponse {
     uint8_t opcode;
     uint32_t ping_id;
 };
 
 struct EvtGetButtonInfoResponse {
     uint8_t opcode;
     bdaddr_t bd_addr;
     // Followed by:
     // uint8_t uuid_length;
     // uint8_t uuid[uuid_length];
     // uint8_t name_length;
     // char name[name_length];
     // int32_t color;
     // uint8_t serial_number_length;
     // char serial_number[serial_number_length];
     // uint8_t flic_version;
     // uint32_t firmware_version;
 };
 
 struct EvtScanWizardFoundPrivateButton {
     uint8_t opcode;
     uint32_t scan_wizard_id;
 };
 
 struct EvtScanWizardFoundPublicButton {
     uint8_t opcode;
     uint32_t scan_wizard_id;
     bdaddr_t bd_addr;
     uint8_t name_length;
     char name[16];
 };
 
 struct EvtScanWizardButtonConnected {
     uint8_t opcode;
     uint32_t scan_wizard_id;
 };
 
 struct EvtScanWizardCompleted {
     uint8_t opcode;
     uint32_t scan_wizard_id;
     uint8_t result;
 };
 
 struct EvtButtonDeleted {
     uint8_t opcode;
     bdaddr_t bd_addr;
     uint8_t deleted_by_this_client;
 };
 
 struct EvtBatteryStatus {
     uint8_t opcode;
     uint32_t listener_id;
     int8_t battery_percentage;
     uint64_t timestamp;
 };
 
 #pragma pack(pop)
 
 #ifdef __cplusplus
 } // namespace FlicClientProtocol
 #endif
 
 #endif // CLIENT_PROTOCOL_PACKETS_H