# MOZA Wheel + Gazebo Self-Driving Demo

## Overview

This project connects a Gazebo simulation to a real MOZA R3 racing wheel using ROS 2, OpenCV, UDP, and DirectInput force feedback.

The goal is to use a camera mounted on an Ackermann steering vehicle inside Gazebo to detect a yellow guide line, compute steering corrections, and physically drive the real steering wheel hands-free.

---

## Current Working State

### Simulation Environment

* Gazebo Harmonic running in WSL (Ubuntu)
* ROS 2 Jazzy for topic communication
* Ackermann steering vehicle with front-mounted camera
* Track uses a yellow line for visual lane following

### Camera Pipeline

The vehicle includes a fixed front camera attached to the chassis.

The camera publishes to:

```text
/debug_camera/image
```

Verified with:

```bash
ros2 topic hz /debug_camera/image
```

---

## Yellow Line Detection

### Python Node

File:

```text
yellow_line_tracking.py
```

Responsibilities:

* Subscribe to `/debug_camera/image`
* Convert image to HSV using OpenCV
* Threshold for yellow
* Compute yellow line centroid
* Calculate steering correction

Output:

```text
steering ∈ [-1.0, +1.0]
```

Meaning:

```text
-1.0 = line far left
 0.0 = centered
+1.0 = line far right
```

Example runtime output:

```text
yellow_cx=470 center=320 error=+0.469 steering=+0.562
```

---

## WSL → Windows Communication

### UDP Bridge

The Python node sends steering values from WSL to Windows over UDP.

Format:

```text
0.3075
```

Destination:

```text
<Windows IPv4 Address>:5005
```

Verified successfully with a Windows UDP receiver.

Example:

```text
('172.24.x.x', 39416) 0.3075
```

---

## MOZA Wheel Force Feedback

### Windows C++ Controller

Responsibilities:

* Receive UDP steering values
* Convert steering values into DirectInput constant force feedback
* Apply force to MOZA R3 wheel

Implementation details:

* DirectInput (`dinput8.lib`, `dxguid.lib`)
* Winsock UDP receiver (`ws2_32.lib`)
* Constant force effect using signed magnitude

Wheel behavior:

```text
positive force = left
negative force = right
```

The system uses continuous updates and falls back to zero force if UDP packets stop.

---

## Current System Architecture

```text
Gazebo (WSL)
    ↓
Front Camera
    ↓
ROS 2 Topic (/debug_camera/image)
    ↓
Python OpenCV Node
    ↓
steering value [-1, +1]
    ↓
UDP
    ↓
Windows C++ Controller
    ↓
DirectInput Force Feedback
    ↓
MOZA R3 Wheel
```

---

## Remaining Objectives

## 1. Validate Full End-to-End Wheel Control

Confirm the physical wheel responds correctly to live steering values from the simulation.

Expected behavior:

* line right → wheel turns right
* line left → wheel turns left
* centered → wheel relaxes

This includes tuning:

* force strength
* force direction sign
* response smoothness

---

## 2. Improve Steering Stability

Optional smoothing for better feel:

```cpp
force = 0.8 * previous + 0.2 * target
```

This reduces aggressive oscillation.

---

## 3. Upgrade to Angle-Based Control (Future)

Current implementation uses force-based steering.

Future improvement:

Use wheel angle tracking and proportional control:

```text
force = (targetAngle - currentAngle) × Kp
```

This would produce more natural steering behavior.

---

## 4. Optional: Control the Simulated Car Too

Use the same steering value to:

* drive the real wheel
* steer the simulated Ackermann vehicle

This creates a full closed-loop demo.

---

## Repository Contents

Typical files:

```text
ackermann_car/model.sdf
worlds/ackermann_adjusted.world
yellow_line_tracking.py
moza_udp_ffb.cpp
README.md
```

---

## Notes

* Close MOZA Pit House before running the C++ controller
* Some systems may require Administrator privileges for DirectInput
* Gazebo must be unpaused for image publishing
* ROS bridge must be running for Gazebo → ROS image transport

Bridge command:

```bash
ros2 run ros_gz_bridge parameter_bridge \
/debug_camera/image@sensor_msgs/msg/Image@gz.msgs.Image
```

---

## Project Status

Core pipeline is working.

The remaining work is primarily final wheel tuning and demonstration polish.
