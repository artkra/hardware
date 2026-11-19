#!/usr/bin/env python3
"""Live 3D orientation visualization for the MPU6050 head tracker.

Reads roll,pitch,yaw (degrees, CSV) from serial and rotates a 3D box to
match in real time, so you can see the sensor's fused orientation directly.
The red face marks "front".

Note: yaw has no absolute reference (no magnetometer on the MPU6050) and
will slowly drift even when the sensor is still — roll/pitch stay accurate
since they're anchored to gravity via the accelerometer.

Setup:
    pip install pyserial matplotlib numpy

Usage:
    python3 scripts/visualize.py [serial_port]
"""
import sys

import matplotlib.pyplot as plt
import numpy as np
import serial
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = 115200

# Box dimensions (front-back, left-right, up-down) — elongated rather than a
# symmetric cube, so the orientation is obvious at a glance.
LENGTH, WIDTH, HEIGHT = 2.0, 1.0, 0.5
VERTICES = np.array([
    [-LENGTH / 2, -WIDTH / 2, -HEIGHT / 2],
    [LENGTH / 2, -WIDTH / 2, -HEIGHT / 2],
    [LENGTH / 2, WIDTH / 2, -HEIGHT / 2],
    [-LENGTH / 2, WIDTH / 2, -HEIGHT / 2],
    [-LENGTH / 2, -WIDTH / 2, HEIGHT / 2],
    [LENGTH / 2, -WIDTH / 2, HEIGHT / 2],
    [LENGTH / 2, WIDTH / 2, HEIGHT / 2],
    [-LENGTH / 2, WIDTH / 2, HEIGHT / 2],
])
FACES_IDX = [
    [0, 1, 2, 3],  # bottom
    [4, 5, 6, 7],  # top
    [1, 2, 6, 5],  # front (+x) — marked red below
    [0, 3, 7, 4],  # back
    [0, 1, 5, 4],  # right
    [2, 3, 7, 6],  # left
]
FACE_COLORS = ["lightgray", "lightgray", "red", "lightgray", "lightgray", "lightgray"]


def rotation_matrix(roll_deg, pitch_deg, yaw_deg):
    r, p, y = np.radians([roll_deg, pitch_deg, yaw_deg])
    rx = np.array([[1, 0, 0], [0, np.cos(r), -np.sin(r)], [0, np.sin(r), np.cos(r)]])
    ry = np.array([[np.cos(p), 0, np.sin(p)], [0, 1, 0], [-np.sin(p), 0, np.cos(p)]])
    rz = np.array([[np.cos(y), -np.sin(y), 0], [np.sin(y), np.cos(y), 0], [0, 0, 1]])
    return rz @ ry @ rx


def read_latest_angles(ser):
    """Drain the serial buffer and return the most recently parsed line."""
    latest = None
    while ser.in_waiting:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        parts = line.split(",")
        if len(parts) == 3:
            try:
                latest = [float(p) for p in parts]
            except ValueError:
                continue
    return latest


def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)

    fig = plt.figure()
    ax = fig.add_subplot(projection="3d")
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.5, 1.5)
    ax.set_zlim(-1.5, 1.5)
    ax.set_box_aspect([1, 1, 1])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_title(f"Head tracker orientation ({PORT})")

    poly = Poly3DCollection(
        [VERTICES[face] for face in FACES_IDX],
        facecolors=FACE_COLORS,
        edgecolors="black",
    )
    ax.add_collection3d(poly)

    def update(_frame):
        angles = read_latest_angles(ser)
        if angles is None:
            return (poly,)
        roll, pitch, yaw = angles
        rot = rotation_matrix(roll, pitch, yaw)
        rotated = VERTICES @ rot.T
        poly.set_verts([rotated[face] for face in FACES_IDX])
        return (poly,)

    ani = FuncAnimation(fig, update, interval=50, blit=False, cache_frame_data=False)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
