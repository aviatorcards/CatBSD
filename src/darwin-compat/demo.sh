#!/bin/bash
# CatBSD Complete Utility Demo - All Darwin utilities!

set -e

echo "╔════════════════════════════════════════════════════════╗"
echo "║                                                        ║"
echo "║   🐱 CatBSD - Complete Darwin Utility Showcase 🐱     ║"
echo "║                                                        ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Show system info
echo "=== System Information (sw_vers) ==="
cd sw_vers-demo
./sw_vers-demo -banner
./sw_vers-demo
cd ..
echo ""

# Demo plutil
echo "=== Property List Utility (plutil) ==="
echo "Creating sample launchd configuration..."
cat > demo.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.catbsd.meow</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/meow</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
EOF

cd plutil-demo
./plutil-demo -lint ../demo.plist
./plutil-demo -p ../demo.plist
cd ..
echo ""

# Demo say
echo "=== Text-to-Speech (say) ==="
cd say-demo
./say-demo "CatBSD version 0.1.0 alpha is ready!"
sleep 1
./say-demo -v Samantha "All Darwin utilities are operational"
sleep 1
cd ..
echo ""

# Demo caffeinate
echo "=== Power Management (caffeinate) ==="
cd caffeinate-demo
./caffeinate-demo -t 3
cd ..
echo ""

# Demo launchd
echo "=== Service Management (launchd) ==="
cd launchd-demo
./launchd-demo help
cd ..
echo ""

echo "╔════════════════════════════════════════════════════════╗"
echo "║                                                        ║"
echo "║   ✅ All 5 Darwin Utilities Working!                   ║"
echo "║                                                        ║"
echo "║   • sw_vers      - System version info                ║"
echo "║   • plutil       - Property list utility              ║"
echo "║   • say          - Text-to-speech                     ║"
echo "║   • caffeinate   - Prevent sleep                      ║"
echo "║   • launchd      - Service management                 ║"
echo "║                                                        ║"
echo "║   CatBSD: Where FreeBSD meets Darwin 🐾               ║"
echo "║                                                        ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
