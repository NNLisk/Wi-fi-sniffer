import time

import pandas as pd
import plotly.express as px
import streamlit as st

from listener import packets, stats


def render_ui():
    st.set_page_config(
        page_title="Packet Sniffer Dashboard",
        layout="wide",
    )

    st.title("Network Packet Sniffer")

    refresh_rate = st.sidebar.slider(
        "Refresh rate (seconds)",
        1,
        10,
        2,
    )

    show_packets = st.sidebar.checkbox(
        "Show packet table",
        value=True,
    )

    col1, col2, col3 = st.columns(3)

    col1.metric("Total Packets", stats["total_packets"])

    col2.metric(
        "Unique Devices",
        len(stats["unique_devices"]),
    )

    col3.metric(
        "Last Batch Size",
        stats["last_batch"],
    )

    df = pd.DataFrame(list(packets))

    if not df.empty:

        left, right = st.columns(2)

        with left:
            st.subheader("RSSI Activity")

            fig = px.line(
                df.tail(200),
                y="rssi",
                title="Recent Signal Strength",
            )

            st.plotly_chart(
                fig,
                use_container_width=True,
            )

        with right:
            st.subheader("Channel Usage")

            ch_df = pd.DataFrame({
                "channel": list(stats["channel_counts"].keys()),
                "count": list(stats["channel_counts"].values()),
            })

            fig2 = px.bar(
                ch_df,
                x="channel",
                y="count",
                title="Packets Per Channel",
            )

            st.plotly_chart(
                fig2,
                use_container_width=True,
            )

        if show_packets:
            st.subheader("Recent Packets")

            st.dataframe(
                df.tail(200).sort_index(ascending=False),
                use_container_width=True,
            )

    else:
        st.info("Waiting for packets...")

    time.sleep(refresh_rate)
    st.rerun()