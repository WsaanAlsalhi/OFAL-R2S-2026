"""
============================================================
OFAL R2S 2026 - MISSION CONTROL
Ground Station Dashboard (Streamlit)
============================================================
"""

import streamlit as st
import serial
import serial.tools.list_ports
import json
import time
import pandas as pd
import plotly.graph_objects as go


# ============================================================
# PAGE CONFIG
# ============================================================
st.set_page_config(
    page_title="OFAL R2S 2026 Mission Control",
    page_icon="🚀",
    layout="wide",
    initial_sidebar_state="expanded"
)


# ============================================================
# CUSTOM CSS
# ============================================================
st.markdown("""
<style>
    .stApp {
        background-color: #050914;
        color: #ffffff;
    }
    [data-testid="stSidebar"] {
        background-color: #0d1424;
        border-right: 1px solid #26334d;
    }
    .metric-card {
        background: #0d1424;
        border: 1px solid #26334d;
        border-radius: 12px;
        padding: 18px;
        text-align: center;
        margin-bottom: 10px;
    }
    .metric-label {
        color: #8899aa;
        font-size: 0.85rem;
        font-weight: 600;
        letter-spacing: 0.5px;
        margin-bottom: 8px;
        text-transform: uppercase;
    }
    .metric-value {
        color: #ffffff;
        font-size: 1.6rem;
        font-weight: bold;
        font-family: 'Courier New', monospace;
    }
    .metric-value.good { color: #00ff9d; }
    .metric-value.warning { color: #ffcc00; }
    .metric-value.danger { color: #ff5c5c; }
    h1, h2, h3 { color: #ffffff !important; }
</style>
""", unsafe_allow_html=True)


# ============================================================
# SESSION STATE
# ============================================================
if "telemetry" not in st.session_state:
    st.session_state.telemetry = []

if "serial_connection" not in st.session_state:
    st.session_state.serial_connection = None

if "connected_port" not in st.session_state:
    st.session_state.connected_port = None


# ============================================================
# HEADER
# ============================================================
st.title("🚀 OFAL R2S 2026 Mission Control")
st.caption("PATRICK • OFAL • 868 MHz LoRa Telemetry")


# ============================================================
# SIDEBAR
# ============================================================
st.sidebar.header("⚙️ Mission Control")

mode = st.sidebar.radio(
    "DATA SOURCE",
    ["DEMO MODE", "LIVE LORA"]
)


# ============================================================
# SERIAL PORTS
# ============================================================
ports = list(serial.tools.list_ports.comports())
port_names = [port.device for port in ports]


# ============================================================
# DEMO DATA
# ============================================================
def generate_demo():
    t = time.time()
    lat = 23.630000 + ((t % 120) / 120) * 0.003
    lon = 58.317000 + ((t % 120) / 120) * 0.003
    altitude = 40 + ((t % 60) / 60) * 100
    speed = 5 + ((t % 20) / 20) * 20

    return {
        "type": "telemetry",
        "rocket": "PATRICK",
        "team": "OFAL",
        "packet": int(t) % 10000,
        "time_ms": int(t * 1000),
        "trigger": "AUTO",
        "gps_fix": True,
        "lat": lat,
        "lon": lon,
        "alt": altitude,
        "speed_kmh": speed,
        "satellites": 11
    }


# ============================================================
# READ FROM LORA RECEIVER
# ============================================================
def read_lora_serial():
    connection = st.session_state.serial_connection

    if connection is None:
        return None

    try:
        if connection.in_waiting:
            line = connection.readline().decode("utf-8", errors="ignore").strip()

            if line.startswith("{"):
                try:
                    return json.loads(line)
                except json.JSONDecodeError:
                    return None
    except Exception:
        st.session_state.serial_connection = None
        st.session_state.connected_port = None
        return None

    return None


# ============================================================
# CONNECTION PANEL
# ============================================================
if mode == "LIVE LORA":
    st.sidebar.subheader("📡 LoRa Receiver")

    if len(port_names) == 0:
        st.sidebar.warning("No serial device detected.")
    else:
        selected_port = st.sidebar.selectbox("Receiver COM Port", port_names)

        col1, col2 = st.sidebar.columns(2)

        with col1:
            if st.button("Connect", use_container_width=True):
                try:
                    if st.session_state.serial_connection is not None:
                        st.session_state.serial_connection.close()

                    st.session_state.serial_connection = serial.Serial(
                        selected_port, 115200, timeout=0.2
                    )
                    st.session_state.connected_port = selected_port
                    st.sidebar.success(f"Connected: {selected_port}")
                except Exception as e:
                    st.sidebar.error(f"Failed: {e}")

        with col2:
            if st.button("Disconnect", use_container_width=True):
                if st.session_state.serial_connection is not None:
                    st.session_state.serial_connection.close()
                    st.session_state.serial_connection = None
                    st.session_state.connected_port = None
                    st.sidebar.info("Disconnected")

        if st.session_state.connected_port:
            st.sidebar.success(f"📡 Connected: {st.session_state.connected_port}")
        else:
            st.sidebar.warning("📡 Not connected")


# ============================================================
# GET DATA
# ============================================================
data = None

if mode == "DEMO MODE":
    data = generate_demo()
else:
    data = read_lora_serial()


if data is not None:
    st.session_state.telemetry.append(data)
    st.session_state.telemetry = st.session_state.telemetry[-500:]


# ============================================================
# STATUS
# ============================================================
if mode == "DEMO MODE":
    st.info("🧪 DEMO MODE ACTIVE — Simulated telemetry")
else:
    if st.session_state.serial_connection is not None:
        st.success("📡 LIVE LORA RECEIVER CONNECTED")
    else:
        st.warning("📡 Please connect the Launch Receiver via Sidebar")


# ============================================================
# CURRENT DATA
# ============================================================
if len(st.session_state.telemetry) > 0:
    current = st.session_state.telemetry[-1]
else:
    current = {
        "packet": 0, "gps_fix": False, "lat": 0, "lon": 0,
        "alt": 0, "speed_kmh": 0, "satellites": 0,
        "time_ms": 0
    }


# ============================================================
# ROW 1
# ============================================================
col1, col2, col3, col4 = st.columns(4)

with col1:
    gps_status = "GPS FIX" if current.get("gps_fix") else "NO FIX"
    gps_color = "good" if current.get("gps_fix") else "danger"
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">GPS STATUS</div>
        <div class="metric-value {gps_color}">{gps_status}</div>
    </div>
    ''', unsafe_allow_html=True)

with col2:
    lora_status = "DEMO" if mode == "DEMO MODE" else "CONNECTED"
    lora_color = "warning" if mode == "DEMO MODE" else "good"
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">LORA LINK</div>
        <div class="metric-value {lora_color}">{lora_status}</div>
    </div>
    ''', unsafe_allow_html=True)

with col3:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">PACKET ID</div>
        <div class="metric-value">{current.get("packet", 0)}</div>
    </div>
    ''', unsafe_allow_html=True)

with col4:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">SATELLITES</div>
        <div class="metric-value">{current.get("satellites", 0)}</div>
    </div>
    ''', unsafe_allow_html=True)


# ============================================================
# ROW 2
# ============================================================
col1, col2, col3, col4 = st.columns(4)

with col1:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">ALTITUDE</div>
        <div class="metric-value">{current.get("alt", 0):.1f} m</div>
    </div>
    ''', unsafe_allow_html=True)

with col2:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">SPEED</div>
        <div class="metric-value">{current.get("speed_kmh", 0):.1f} km/h</div>
    </div>
    ''', unsafe_allow_html=True)

with col3:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">LATITUDE</div>
        <div class="metric-value">{current.get("lat", 0):.5f}</div>
    </div>
    ''', unsafe_allow_html=True)

with col4:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-label">LONGITUDE</div>
        <div class="metric-value">{current.get("lon", 0):.5f}</div>
    </div>
    ''', unsafe_allow_html=True)


# ============================================================
# MAP
# ============================================================
st.subheader("🗺️ Rocket Location & Trajectory")

lat = current.get("lat", 0)
lon = current.get("lon", 0)

if lat != 0 and lon != 0:
    fig = go.Figure()

    if len(st.session_state.telemetry) > 1:
        track_df = pd.DataFrame(st.session_state.telemetry)
        track_df = track_df[(track_df["lat"] != 0) & (track_df["lon"] != 0)]

        if len(track_df) > 0:
            fig.add_trace(go.Scattermap(
                lat=track_df["lat"],
                lon=track_df["lon"],
                mode="lines+markers",
                line=dict(width=3, color="#00ff9d"),
                marker=dict(size=6, color="#00ff9d"),
                name="Rocket Track"
            ))

    fig.add_trace(go.Scattermap(
        lat=[lat],
        lon=[lon],
        mode="markers",
        marker=dict(size=20, color="#ff5c5c"),
        name="PATRICK"
    ))

    fig.update_layout(
        map=dict(style="open-street-map",
                 center=dict(lat=lat, lon=lon),
                 zoom=14),
        height=500,
        margin=dict(l=0, r=0, t=0, b=0),
        paper_bgcolor="#050914"
    )

    st.plotly_chart(fig, use_container_width=True)
else:
    st.info("Waiting for valid GPS coordinates...")


# ============================================================
# LOG
# ============================================================
st.subheader("📡 Telemetry Log (Last 20)")

if len(st.session_state.telemetry) > 0:
    df = pd.DataFrame(st.session_state.telemetry)
    display_cols = ["packet", "gps_fix", "lat", "lon",
                    "alt", "speed_kmh", "satellites"]
    available_cols = [c for c in display_cols if c in df.columns]
    st.dataframe(df[available_cols].tail(20),
                 use_container_width=True, hide_index=True)


# ============================================================
# MISSION INFO
# ============================================================
st.divider()
col1, col2, col3 = st.columns(3)
with col1:
    st.markdown("**🚀 Rocket:** PATRICK")
with col2:
    st.markdown("**👥 Team:** OFAL")
with col3:
    st.markdown("**📡 Telemetry:** LoRa 868 MHz")

st.caption("SMALL ROCKET • BIG DREAMS | OFAL R2S 2026")


# ============================================================
# AUTO REFRESH
# ============================================================
time.sleep(0.5)
st.rerun()
