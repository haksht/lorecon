#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Conference Demo Launcher

One-command setup for impressive conference demonstrations.
Launches visualizer, optionally opens web UI, and handles recording.

Usage:
    python demo.py COM3
    python demo.py COM3 --web --record --duration 300
    python demo.py --list-ports
"""

import argparse
import subprocess
import sys
import time
import webbrowser
from pathlib import Path

try:
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    print("[!] pyserial not installed. Install with: pip install pyserial")

def list_serial_ports():
    """List available serial ports"""
    if not SERIAL_AVAILABLE:
        print("[!] Cannot list ports without pyserial")
        return []
    
    ports = list(serial.tools.list_ports.comports())
    
    if not ports:
        print("[!] No serial ports found")
        return []
    
    print("\n📡 Available Serial Ports:\n")
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port.device}")
        print(f"     Description: {port.description}")
        print(f"     Manufacturer: {port.manufacturer or 'Unknown'}")
        print()
    
    return ports

def detect_esp32():
    """Try to auto-detect ESP32 port"""
    if not SERIAL_AVAILABLE:
        return None
    
    ports = list(serial.tools.list_ports.comports())
    
    # Look for common ESP32 identifiers
    for port in ports:
        desc_lower = (port.description or '').lower()
        mfr_lower = (port.manufacturer or '').lower()
        
        if any(kw in desc_lower for kw in ['cp210', 'ch340', 'esp32', 'uart']):
            return port.device
        if any(kw in mfr_lower for kw in ['silicon labs', 'wch', 'espressif']):
            return port.device
    
    return None

def wait_for_esp32(port, timeout=10):
    """Wait for ESP32 to be ready"""
    print(f"[*] Waiting for ESP32 on {port}...")
    
    if not SERIAL_AVAILABLE:
        print("[!] Cannot verify port without pyserial")
        time.sleep(2)
        return True
    
    start = time.time()
    while time.time() - start < timeout:
        try:
            ser = serial.Serial(port, 115200, timeout=1)
            ser.close()
            print("[✓] ESP32 ready")
            return True
        except serial.SerialException:
            time.sleep(0.5)
    
    print("[!] Timeout waiting for ESP32")
    return False

def launch_visualizer(port, audio=False, record=False, json_mode=False):
    """Launch the enhanced visualizer"""
    print("\n[*] Launching enhanced live visualizer...")
    
    script = Path(__file__).parent / 'enhanced_live_visualizer.py'
    
    if not script.exists():
        print(f"[!] Visualizer not found: {script}")
        return None
    
    cmd = [sys.executable, str(script), port]
    
    if audio:
        cmd.append('--audio')
    if record:
        cmd.append('--record')
    if json_mode:
        cmd.append('--json')
    
    try:
        proc = subprocess.Popen(cmd)
        print(f"[✓] Visualizer launched (PID: {proc.pid})")
        return proc
    except Exception as e:
        print(f"[!] Failed to launch visualizer: {e}")
        return None

def open_web_ui(esp32_ip='192.168.4.1'):
    """Open web UI in default browser"""
    print("\n[*] Opening web interface...")
    url = f"http://{esp32_ip}"
    
    try:
        webbrowser.open(url)
        print(f"[✓] Web UI opened: {url}")
        return True
    except Exception as e:
        print(f"[!] Could not open browser: {e}")
        print(f"[!] Manually navigate to: {url}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Conference Demo Launcher for ESP32 LoRa Sniffer',
        epilog='Example: python demo.py COM3 --web --audio --record'
    )
    
    parser.add_argument('port', nargs='?', help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--list-ports', action='store_true',
                       help='List available serial ports and exit')
    parser.add_argument('--auto-detect', action='store_true',
                       help='Auto-detect ESP32 port')
    parser.add_argument('--web', action='store_true',
                       help='Open web UI in browser')
    parser.add_argument('--web-ip', default='192.168.4.1',
                       help='ESP32 web UI IP address (default: 192.168.4.1)')
    parser.add_argument('--audio', action='store_true',
                       help='Enable audio feedback (Geiger counter effect)')
    parser.add_argument('--record', action='store_true',
                       help='Enable screenshot recording')
    parser.add_argument('--json', action='store_true',
                       help='Enable JSON output mode')
    parser.add_argument('--duration', type=int, metavar='SECONDS',
                       help='Auto-exit after duration (for scripted demos)')
    
    args = parser.parse_args()
    
    print("="*70)
    print("  🎯 ESP32 LoRa Sniffer - Conference Demo Launcher")
    print("="*70)
    print()
    
    # Handle --list-ports
    if args.list_ports:
        list_serial_ports()
        return 0
    
    # Determine port
    port = args.port
    
    if args.auto_detect:
        print("[*] Auto-detecting ESP32...")
        detected = detect_esp32()
        if detected:
            print(f"[✓] Found ESP32 on: {detected}")
            port = detected
        else:
            print("[!] Auto-detection failed")
            if not port:
                list_serial_ports()
                return 1
    
    if not port:
        print("[!] No port specified. Use --list-ports to see available ports.")
        print()
        print("Usage examples:")
        print("  python demo.py COM3")
        print("  python demo.py /dev/ttyUSB0 --audio --record")
        print("  python demo.py --auto-detect --web")
        return 1
    
    # Wait for ESP32 to be ready
    if not wait_for_esp32(port):
        print("[!] ESP32 not responding. Check connection and power.")
        return 1
    
    # Launch components
    visualizer_proc = None
    
    try:
        # Open web UI if requested
        if args.web:
            open_web_ui(args.web_ip)
            time.sleep(1)
        
        # Launch visualizer
        visualizer_proc = launch_visualizer(
            port, 
            audio=args.audio,
            record=args.record,
            json_mode=args.json
        )
        
        if not visualizer_proc:
            return 1
        
        print("\n" + "="*70)
        print("  ✅ Demo Ready!")
        print("="*70)
        print()
        print("  Visual:    Matplotlib window")
        if args.web:
            print(f"  Web UI:    http://{args.web_ip}")
        if args.audio:
            print("  Audio:     Enabled (Geiger counter effect)")
        if args.record:
            print("  Recording: Screenshots in screenshots/")
        print()
        print("  Press Ctrl+C to stop")
        print("="*70)
        print()
        
        # Wait for specified duration or user interrupt
        if args.duration:
            print(f"[*] Running for {args.duration} seconds...")
            time.sleep(args.duration)
            print("[*] Duration complete, exiting...")
        else:
            # Wait indefinitely
            visualizer_proc.wait()
    
    except KeyboardInterrupt:
        print("\n[*] Demo stopped by user")
    
    finally:
        # Cleanup
        if visualizer_proc and visualizer_proc.poll() is None:
            print("[*] Stopping visualizer...")
            visualizer_proc.terminate()
            try:
                visualizer_proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                visualizer_proc.kill()
        
        print("[✓] Cleanup complete")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
