import sqlite3
import pandas as pd

def init_db():
    conn = sqlite3.connect('packets.db')
    conn.execute('''CREATE TABLE IF NOT EXISTS packets (
        timestamp_ms INTEGER,
        frame_control INTEGER,
        rssi INTEGER,
        mac TEXT,
        channel INTEGER,
        received REAL
    )''')
    conn.commit()
    conn.close()

def save_packet(packet):
    conn = sqlite3.connect('packets.db')
    conn.execute('INSERT INTO packets VALUES (?,?,?,?,?,?)',
        (packet['timestamp_ms'], packet['frame_control'], 
         packet['rssi'], packet['mac'], 
         packet['channel'], packet['received']))
    conn.commit()
    conn.close()


def get_recent_packets(n=500):
    # print("DB getting packets")
    conn = sqlite3.connect('packets.db')
    df = pd.read_sql(
        'SELECT * FROM packets ORDER BY received DESC LIMIT 5000',
        conn
    )
    conn.close()
    return df

def get_stats():
    conn = sqlite3.connect('packets.db')
    total = conn.execute('SELECT COUNT(*) FROM packets').fetchone()[0]
    unique = conn.execute('SELECT COUNT(DISTINCT mac) FROM packets').fetchone()[0]
    channel_counts = dict(conn.execute(
        'SELECT channel, COUNT(*) FROM packets GROUP BY channel'
    ).fetchall())
    conn.close()
    return {
        "total_packets": total,
        "unique_devices": unique,
        "channel_counts": channel_counts,
    }