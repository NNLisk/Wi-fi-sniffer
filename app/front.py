import streamlit as st
import pandas as pd
import sqlite3
from streamlit_autorefresh import st_autorefresh
from db import get_recent_packets

DB_PATH = "packets.db"

# auto refresh every 2 seconds
st_autorefresh(interval=2000, key="packet_refresh")

st.set_page_config(
    page_title="Packet Monitor",
    layout="wide"
)

st.title("Live Packet Monitor")


@st.cache_data(ttl=2)
def load_packets(limit=500):
    conn = sqlite3.connect(DB_PATH)

    query = f"""
    SELECT
        timestamp_ms,
        frame_control,
        rssi,
        mac,
        channel,
        received
    FROM packets
    ORDER BY received DESC
    LIMIT {limit}
    """

    df = pd.read_sql_query(query, conn)

    conn.close()

    return df


df = get_recent_packets()

if df.empty:
    st.warning("No packets received yet.")
    st.stop()


# statistics
col1, col2, col3, col4 = st.columns(4)

with col1:
    st.metric("Packets", len(df))

with col2:
    st.metric("Unique MACs", df["mac"].nunique())

with col3:
    st.metric("Channels", df["channel"].nunique())

with col4:
    st.metric("Strongest RSSI", int(df["rssi"].max()))


st.divider()


#RSSI of packets received over time
st.subheader("RSSI Over Time")

chart_df = df.copy()

chart_df["received"] = pd.to_datetime(
    chart_df["received"],
    unit="s"
)

chart_df = chart_df.sort_values("received")

st.line_chart(
    chart_df.set_index("received")["rssi"]
)


# channel dist.
st.subheader("Channel Distribution")

channel_counts = (
    df["channel"]
    .value_counts()
    .sort_index()
)

st.bar_chart(channel_counts)


#latest packets
st.subheader("Latest Packets")

st.dataframe(
    df,
    width="stretch",
    hide_index=True
)