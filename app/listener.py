import socket
import struct
import time
import logging

from db import save_packet, init_db

HOST = "0.0.0.0"
PORT = 58585

PACKET_SIZE = 4 + 2 + 1 + 6 + 1


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)


def recv_exact(conn, n):
    buf = b""

    while len(buf) < n:
        chunk = conn.recv(n - len(buf))

        if not chunk:
            raise ConnectionError("Connection closed")

        buf += chunk

    return buf


def handle_client(conn, addr):
    logging.info(f"Connected: {addr}")

    try:
        count_data = recv_exact(conn, 4)
        count = struct.unpack("<I", count_data)[0]

        logging.info(f"Receiving {count} packets")

        for _ in range(count):
            try:
                data = recv_exact(conn, PACKET_SIZE)

                ts, fc, rssi, mac, ch = struct.unpack(
                    "<IHb6sB",
                    data
                )

                packet = {
                    "timestamp_ms": ts,
                    "frame_control": fc,
                    "rssi": rssi,
                    "mac": mac.hex(":"),
                    "channel": ch,
                    "received": time.time(),
                }

                save_packet(packet)

            except Exception as e:
                logging.exception("Failed to process packet")

    except Exception:
        logging.exception("Client connection failed")

    finally:
        conn.close()
        logging.info(f"Disconnected: {addr}")


def start_listener():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    server.bind((HOST, PORT))
    server.listen(5)

    logging.info(f"Listening on {HOST}:{PORT}")

    while True:
        try:
            conn, addr = server.accept()
            handle_client(conn, addr)

        except KeyboardInterrupt:
            logging.info("Shutting down listener")
            break

        except Exception:
            logging.exception("Server loop error")

    server.close()


if __name__ == "__main__":
    init_db()
    start_listener()