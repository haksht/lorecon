#!/usr/bin/env python3
"""
ESP32 LoRa Sniffer - Packet Differ

Side-by-side hex diff of two packets for reverse engineering.
Highlights byte differences with color coding and offset annotations.

Useful for:
- Comparing encrypted vs decrypted packets
- Analyzing protocol variations
- Identifying changing fields (counters, timestamps)
- Reverse engineering unknown protocols

Usage:
    python packet_differ.py packet1.hex packet2.hex
    python packet_differ.py --hex "AABBCC" "AABBDD"
    echo "AABBCC" | python packet_differ.py --stdin "AABBDD"
    python packet_differ.py capture.csv --packets 1,5

Requirements: None (pure Python)
"""

import argparse
import sys
from pathlib import Path
from typing import List, Optional, Tuple


# ANSI color codes
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    GRAY = '\033[90m'
    RESET = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    
    # Background
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'


def detect_color_support() -> bool:
    """Check if terminal supports colors"""
    if sys.platform == 'win32':
        try:
            import ctypes
            kernel32 = ctypes.windll.kernel32
            kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
            return True
        except Exception:
            return False
    return hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()


def parse_hex_string(hex_str: str) -> bytes:
    """Parse various hex string formats to bytes"""
    # Remove common prefixes and separators
    clean = hex_str.strip()
    clean = clean.replace('0x', '').replace('0X', '')
    clean = clean.replace(' ', '').replace(':', '').replace('-', '')
    clean = clean.replace('\n', '').replace('\r', '')
    
    # Handle odd length
    if len(clean) % 2 != 0:
        clean = '0' + clean
    
    return bytes.fromhex(clean)


def format_hex_byte(byte_val: int, highlight: bool = False, 
                   use_color: bool = True, is_a: bool = True) -> str:
    """Format a single byte with optional highlighting"""
    hex_str = f'{byte_val:02X}'
    
    if not use_color:
        return f'[{hex_str}]' if highlight else hex_str
    
    if highlight:
        # Different colors for A vs B
        if is_a:
            return f'{Colors.BG_RED}{Colors.WHITE}{hex_str}{Colors.RESET}'
        else:
            return f'{Colors.BG_GREEN}{Colors.WHITE}{hex_str}{Colors.RESET}'
    return hex_str


def format_ascii_byte(byte_val: int, highlight: bool = False,
                     use_color: bool = True, is_a: bool = True) -> str:
    """Format byte as ASCII with highlighting"""
    if 32 <= byte_val <= 126:
        char = chr(byte_val)
    else:
        char = '.'
    
    if not use_color:
        return f'[{char}]' if highlight else char
    
    if highlight:
        if is_a:
            return f'{Colors.BG_RED}{Colors.WHITE}{char}{Colors.RESET}'
        else:
            return f'{Colors.BG_GREEN}{Colors.WHITE}{char}{Colors.RESET}'
    
    if byte_val < 32 or byte_val > 126:
        return f'{Colors.GRAY}.{Colors.RESET}'
    return char


class PacketDiffer:
    """Compare two packets byte-by-byte"""
    
    def __init__(self, use_color: bool = True, bytes_per_line: int = 16):
        self.use_color = use_color
        self.bytes_per_line = bytes_per_line
        self.diff_count = 0
        self.diff_positions: List[int] = []
    
    def diff(self, packet_a: bytes, packet_b: bytes) -> str:
        """Generate side-by-side diff output"""
        self.diff_count = 0
        self.diff_positions = []
        
        lines = []
        max_len = max(len(packet_a), len(packet_b))
        
        # Header
        lines.append(self._format_header(len(packet_a), len(packet_b)))
        lines.append('')
        
        # Process in chunks
        for offset in range(0, max_len, self.bytes_per_line):
            chunk_a = packet_a[offset:offset + self.bytes_per_line]
            chunk_b = packet_b[offset:offset + self.bytes_per_line]
            
            hex_line_a, ascii_line_a, hex_line_b, ascii_line_b, diffs = \
                self._format_chunk(offset, chunk_a, chunk_b)
            
            self.diff_count += diffs
            
            # Format offset
            offset_str = f'{offset:04X}'
            if self.use_color:
                offset_str = f'{Colors.CYAN}{offset_str}{Colors.RESET}'
            
            # Build output lines
            lines.append(f'{offset_str}  A: {hex_line_a}  |{ascii_line_a}|')
            lines.append(f'      B: {hex_line_b}  |{ascii_line_b}|')
            
            if diffs > 0:
                lines.append('')
        
        # Summary
        lines.append('')
        lines.append(self._format_summary(len(packet_a), len(packet_b)))
        
        return '\n'.join(lines)
    
    def _format_header(self, len_a: int, len_b: int) -> str:
        """Format diff header"""
        header = []
        header.append('=' * 70)
        header.append('PACKET DIFF ANALYSIS')
        header.append('=' * 70)
        
        if self.use_color:
            header.append(f'Packet A: {Colors.RED}{len_a} bytes{Colors.RESET}')
            header.append(f'Packet B: {Colors.GREEN}{len_b} bytes{Colors.RESET}')
        else:
            header.append(f'Packet A: {len_a} bytes')
            header.append(f'Packet B: {len_b} bytes')
        
        if len_a != len_b:
            diff = abs(len_a - len_b)
            header.append(f'Length difference: {diff} bytes')
        
        header.append('')
        header.append('Legend: Highlighted bytes differ between packets')
        header.append('-' * 70)
        
        return '\n'.join(header)
    
    def _format_chunk(self, offset: int, chunk_a: bytes, chunk_b: bytes) \
            -> Tuple[str, str, str, str, int]:
        """Format a chunk of bytes with diff highlighting"""
        hex_parts_a = []
        hex_parts_b = []
        ascii_parts_a = []
        ascii_parts_b = []
        diff_count = 0
        
        max_chunk = max(len(chunk_a), len(chunk_b))
        
        for i in range(self.bytes_per_line):
            if i < max_chunk:
                byte_a = chunk_a[i] if i < len(chunk_a) else None
                byte_b = chunk_b[i] if i < len(chunk_b) else None
                
                # Check if different
                is_diff = byte_a != byte_b
                if is_diff:
                    diff_count += 1
                    self.diff_positions.append(offset + i)
                
                # Format hex
                if byte_a is not None:
                    hex_parts_a.append(format_hex_byte(
                        byte_a, is_diff, self.use_color, is_a=True))
                    ascii_parts_a.append(format_ascii_byte(
                        byte_a, is_diff, self.use_color, is_a=True))
                else:
                    hex_parts_a.append('  ')
                    ascii_parts_a.append(' ')
                
                if byte_b is not None:
                    hex_parts_b.append(format_hex_byte(
                        byte_b, is_diff, self.use_color, is_a=False))
                    ascii_parts_b.append(format_ascii_byte(
                        byte_b, is_diff, self.use_color, is_a=False))
                else:
                    hex_parts_b.append('  ')
                    ascii_parts_b.append(' ')
            else:
                hex_parts_a.append('  ')
                hex_parts_b.append('  ')
                ascii_parts_a.append(' ')
                ascii_parts_b.append(' ')
            
            # Add spacing every 8 bytes
            if i == 7:
                hex_parts_a.append(' ')
                hex_parts_b.append(' ')
        
        return (
            ' '.join(hex_parts_a),
            ''.join(ascii_parts_a),
            ' '.join(hex_parts_b),
            ''.join(ascii_parts_b),
            diff_count
        )
    
    def _format_summary(self, len_a: int, len_b: int) -> str:
        """Format diff summary"""
        lines = ['-' * 70]
        
        total_bytes = max(len_a, len_b)
        match_pct = ((total_bytes - self.diff_count) / total_bytes * 100) if total_bytes > 0 else 100
        
        if self.use_color:
            if self.diff_count == 0:
                lines.append(f'{Colors.GREEN}✓ Packets are identical{Colors.RESET}')
            else:
                lines.append(f'{Colors.YELLOW}⚠ {self.diff_count} byte(s) differ '
                           f'({match_pct:.1f}% match){Colors.RESET}')
        else:
            if self.diff_count == 0:
                lines.append('Packets are identical')
            else:
                lines.append(f'{self.diff_count} byte(s) differ ({match_pct:.1f}% match)')
        
        # Show diff positions
        if self.diff_positions:
            positions = ', '.join(f'0x{p:02X}' for p in self.diff_positions[:20])
            if len(self.diff_positions) > 20:
                positions += f', ... ({len(self.diff_positions) - 20} more)'
            lines.append(f'Diff positions: {positions}')
        
        lines.append('=' * 70)
        return '\n'.join(lines)
    
    def diff_inline(self, packet_a: bytes, packet_b: bytes) -> str:
        """Generate inline diff (compact format)"""
        self.diff_count = 0
        self.diff_positions = []
        
        lines = []
        lines.append(f'A ({len(packet_a)} bytes): ', )
        lines.append(f'B ({len(packet_b)} bytes): ')
        
        hex_a = []
        hex_b = []
        
        max_len = max(len(packet_a), len(packet_b))
        
        for i in range(max_len):
            byte_a = packet_a[i] if i < len(packet_a) else None
            byte_b = packet_b[i] if i < len(packet_b) else None
            
            is_diff = byte_a != byte_b
            if is_diff:
                self.diff_count += 1
            
            if byte_a is not None:
                hex_a.append(format_hex_byte(byte_a, is_diff, self.use_color, True))
            else:
                hex_a.append('--')
            
            if byte_b is not None:
                hex_b.append(format_hex_byte(byte_b, is_diff, self.use_color, False))
            else:
                hex_b.append('--')
        
        return f"A: {' '.join(hex_a)}\nB: {' '.join(hex_b)}\n\n{self.diff_count} differences"


def load_from_csv(filepath: Path, packet_indices: List[int]) -> List[bytes]:
    """Load specific packets from CSV file"""
    import csv
    
    packets = []
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        
        for idx in packet_indices:
            if idx < 0:
                idx = len(rows) + idx  # Support negative indexing
            
            if 0 <= idx < len(rows):
                row = rows[idx]
                raw_hex = row.get('raw', row.get('data', row.get('payload', '')))
                if raw_hex:
                    try:
                        packets.append(parse_hex_string(raw_hex))
                    except ValueError:
                        print(f"Warning: Could not parse packet at index {idx}")
            else:
                print(f"Warning: Packet index {idx} out of range (0-{len(rows)-1})")
    
    return packets


def load_from_file(filepath: Path) -> bytes:
    """Load packet data from file"""
    with open(filepath, 'rb') as f:
        content = f.read()
    
    # Try to detect if it's hex text or binary
    try:
        # If it decodes as text and contains only hex chars, treat as hex
        text = content.decode('utf-8').strip()
        if all(c in '0123456789abcdefABCDEF \n\r:-' for c in text):
            return parse_hex_string(text)
    except UnicodeDecodeError:
        pass
    
    # Otherwise treat as raw binary
    return content


def main():
    parser = argparse.ArgumentParser(
        description='Side-by-side packet diff for reverse engineering',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    %(prog)s packet1.bin packet2.bin        # Compare binary files
    %(prog)s --hex "AABBCC" "AABBDD"        # Compare hex strings
    %(prog)s capture.csv --packets 0,5      # Compare packets from CSV
    %(prog)s --hex "FF0102" --stdin         # Compare with stdin
    
Color Coding:
    Red background  = Byte differs in packet A
    Green background = Byte differs in packet B
    
Hex Input Formats:
    AABBCC, AA BB CC, AA:BB:CC, 0xAA0xBB, aa-bb-cc
'''
    )
    
    parser.add_argument('input1', nargs='?', help='First packet (file or hex string)')
    parser.add_argument('input2', nargs='?', help='Second packet (file or hex string)')
    
    parser.add_argument('--hex', action='store_true',
                       help='Treat inputs as hex strings instead of files')
    parser.add_argument('--stdin', action='store_true',
                       help='Read first packet from stdin')
    parser.add_argument('--packets', metavar='I,J',
                       help='Packet indices to compare from CSV (e.g., 0,5 or 1,-1)')
    parser.add_argument('--compact', action='store_true',
                       help='Use compact inline diff format')
    parser.add_argument('--no-color', action='store_true',
                       help='Disable colored output')
    parser.add_argument('-w', '--width', type=int, default=16,
                       help='Bytes per line (default: 16)')
    
    args = parser.parse_args()
    
    # Determine color support
    use_color = not args.no_color and detect_color_support()
    
    differ = PacketDiffer(use_color=use_color, bytes_per_line=args.width)
    
    # Handle CSV packet comparison
    if args.packets:
        if not args.input1:
            parser.error("CSV file required when using --packets")
        
        indices = [int(x.strip()) for x in args.packets.split(',')]
        if len(indices) != 2:
            parser.error("--packets requires exactly 2 indices (e.g., --packets 0,5)")
        
        csv_path = Path(args.input1)
        if not csv_path.exists():
            print(f"ERROR: File not found: {csv_path}")
            sys.exit(1)
        
        packets = load_from_csv(csv_path, indices)
        if len(packets) != 2:
            print("ERROR: Could not load 2 packets from CSV")
            sys.exit(1)
        
        packet_a, packet_b = packets
    
    else:
        # Load packets from inputs
        if args.stdin:
            stdin_data = sys.stdin.read().strip()
            packet_a = parse_hex_string(stdin_data)
            
            if args.hex:
                packet_b = parse_hex_string(args.input1 or args.input2)
            else:
                if not args.input1:
                    parser.error("Second input required")
                packet_b = load_from_file(Path(args.input1))
        
        elif args.hex:
            if not args.input1 or not args.input2:
                parser.error("Two hex strings required")
            packet_a = parse_hex_string(args.input1)
            packet_b = parse_hex_string(args.input2)
        
        else:
            if not args.input1 or not args.input2:
                parser.print_help()
                sys.exit(1)
            
            path_a = Path(args.input1)
            path_b = Path(args.input2)
            
            if not path_a.exists():
                print(f"ERROR: File not found: {path_a}")
                sys.exit(1)
            if not path_b.exists():
                print(f"ERROR: File not found: {path_b}")
                sys.exit(1)
            
            packet_a = load_from_file(path_a)
            packet_b = load_from_file(path_b)
    
    # Generate diff
    if args.compact:
        output = differ.diff_inline(packet_a, packet_b)
    else:
        output = differ.diff(packet_a, packet_b)
    
    print(output)
    
    # Exit code: 0 if identical, 1 if different
    sys.exit(0 if differ.diff_count == 0 else 1)


if __name__ == '__main__':
    main()
