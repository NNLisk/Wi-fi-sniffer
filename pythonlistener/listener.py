import socket, struct

# listener, receivers and prints the network packets

HOST = '0.0.0.0'
PORT = 58585  

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(1)
    print(f"Listening on {PORT}...")

    conn, addr = s.accept()
    print(f"Connected: {addr}")

    count_data = conn.recv(4)
    count = struct.unpack('<I', count_data)[0]
    print(f"Expecting {count} packets")

    packet_size = 4 + 1 + 1 + 6 + 1  # matches packet_log_t
    for i in range(count):
        data = conn.recv(packet_size)
        ts, fc, rssi, mac, ch = struct.unpack('<IBB6sB', data)  # adjust if needed
        print(f"[{ts}ms] ch={ch} rssi={rssi} mac={mac.hex(':')} fc={fc:#04x}")

    conn.close()

main()