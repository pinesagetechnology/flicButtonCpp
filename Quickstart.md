# Quick Start Guide - Flic C++ Client

## ✅ Compilation Successful!

Your Flic C++ client has been successfully compiled. Here's everything you need:

## 📁 Files Included

1. **flic_client** - Compiled executable (ready to run)
2. **flic_client.cpp** - Source code
3. **client_protocol_packets.h** - Protocol header file
4. **Makefile** - Build configuration  
5. **README.md** - Full documentation

## 🚀 Running the Client

### Prerequisites
Make sure you have the `flicd` daemon running:

```bash
# Download flicd if you haven't already
git clone https://github.com/50ButtonsEach/fliclib-linux-hci.git
cd fliclib-linux-hci/bin/x86_64  # or your architecture

# Give permissions
sudo setcap cap_net_admin=ep ./flicd

# Start the daemon
./flicd -f flic.sqlite3
```

### Run the Client

```bash
# Make the binary executable
chmod +x flic_client

# Connect to local flicd server
./flic_client localhost

# Or connect to a specific host/port
./flic_client 192.168.1.100 5551
```

## 🎯 First Time Setup - Pairing a Button

1. Start the client:
   ```bash
   ./flic_client localhost
   ```

2. Start the scan wizard:
   ```
   > startScanWizard
   ```

3. Press and hold your Flic button for 7 seconds

4. Once paired, connect to it:
   ```
   > connect 80:e4:da:71:3b:ff 1
   ```
   (Replace the MAC address with your button's address shown during pairing)

5. Press your button to see events!

## 📋 Common Commands

```bash
getInfo              # Show server status and verified buttons
startScanWizard      # Pair a new button
startScan            # Scan for buttons (shows advertisements)
stopScan             # Stop scanning
connect <addr> <id>  # Connect to a button
disconnect <id>      # Disconnect
help                 # Show all commands
quit                 # Exit
```

## 🔧 Rebuilding from Source

If you need to modify and recompile:

```bash
# Edit the source
vim flic_client.cpp

# Recompile
make

# Or compile manually
g++ -std=c++11 -Wall -Wextra -O2 -o flic_client flic_client.cpp
```

## 💡 Example Session

```bash
$ ./flic_client localhost
Connected to Flic server at localhost:5551

=== Server Info ===
Bluetooth controller state: Attached
My BD Address: b8:27:eb:12:34:56 (Public)
Max pending connections: 128
Max concurrent connections: 10
Verified buttons:
  80:e4:da:71:3b:ff

> connect 80:e4:da:71:3b:ff 1
Connecting to 80:e4:da:71:3b:ff...
Connection status changed for 80:e4:da:71:3b:ff (conn_id: 1): Ready

# Now press your button!
Button DOWN (conn_id: 1, age: 0 ms)
Button UP (conn_id: 1, age: 105 ms)
Button CLICK (conn_id: 1, age: 105 ms)

> disconnect 1
> quit
```

## 🐛 Troubleshooting

### "Failed to connect"
- Ensure flicd is running: `ps aux | grep flicd`
- Check the port (default is 5551)
- Verify firewall settings

### "Button is private"
- Button is paired with another device
- Hold button for 7+ seconds to reset
- Use `forceDisconnect <addr>` command

### Permission Errors
```bash
# Give flicd the necessary capabilities
sudo setcap cap_net_admin=ep ./flicd

# Or run as root (not recommended)
sudo ./flicd -f flic.sqlite3
```

## 📚 Learn More

- Full README: See README.md for complete documentation
- Protocol Docs: https://github.com/50ButtonsEach/fliclib-linux-hci/blob/master/ProtocolDocumentation.md
- Flic SDK: https://github.com/50ButtonsEach/fliclib-linux-hci

## 🎉 You're Ready!

Your Flic C++ client is compiled and ready to use. Simply run it and start interacting with your Flic buttons!

For more details, see the full README.md file.