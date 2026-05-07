import socket, struct, time
from collections import deque

# listener, receivers and prints the network packets

HOST = '0.0.0.0'
PORT = 58585  

packets = deque(maxlen=5000)
packet_size = 4 + 1 + 1 + 6 + 1  # matches packet_log_t


stats = {
    "total_packets": 0,
    "last_batch": 0,
    "unique_devices": set(),
    "channel_counts": {},
}


def start_listener():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(5)
    
    print(f"Listening on {PORT}...")

    while True:
        conn, addr = s.accept()
        print(f"Connected: {addr}")

        try:
            count_data = recv_exact(conn, 4)

            if not count_data:
                conn.close()
                continue

            count = struct.unpack('<I', count_data)[0]
            print(f"Expecting {count} packets")

            for i in range(count):

                data = recv_exact(conn, packet_size)
                if not data:
                    break

                ts, fc, rssi, mac, ch = struct.unpack('<IBB6sB', data)
                mac_str = mac.hex(":")
                
                # print(f"[{ts}ms] ch={ch} rssi={rssi} mac={mac.hex(':')} fc={fc:#04x}")

                packet = {
                    "timestamp_ms": ts,
                    "frame_control": fc,
                    "rssi": rssi,
                    "mac": mac,
                    "channel": ch,
                    "received": time.time()
                }

                packets.append(packet)
                stats[total_packets] + 1
                stats[unique_devices].add(mac_str)

                if ch not in stats[channel_counts]:
                    stats["channel_counts"][ch] = 0
                stats["channel_counts"][ch] += 1
        except Exception as e:
            print("bad row")

        conn.close()

def recv_exact(conn, n):
    buf = b''
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Connection closed")
        buf += chunk
    return buf

