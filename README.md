# MOZA Wheel + Gazebo Self-Driving Demo

## Motivation / Overview

**Motivation:**

TODO: Add personal motivation here.

This project demonstrates a physical-feedback self-driving simulation pipeline that connects a Gazebo vehicle simulation to a real MOZA R3 racing wheel. The simulated vehicle uses a camera mounted on an Ackermann steering car to follow a yellow line on a Gazebo track. A Python ROS 2 node processes the camera feed with OpenCV, computes a steering command, drives the simulated car through `/cmd_vel`, and simultaneously sends that steering command over UDP to a Windows C++ DirectInput program that physically turns the MOZA wheel.

At a high level, the project combines:

* Gazebo Harmonic simulation
* ROS 2 Jazzy topic communication
* OpenCV image processing
* UDP communication between WSL and Windows
* Windows DirectInput force feedback control
* A real MOZA R3 racing wheel and pedals

The result is a mixed simulation/hardware demonstration: the vehicle drives around a track in Gazebo while the real steering wheel moves in response to the simulated car's line-following behavior.

---

## Demonstration

### YouTube Demo

TODO: Add YouTube video link here.

Example placeholder:

```text
https://www.youtube.com/watch?v=YOUR_VIDEO_ID
```

### What the Demo Shows

The final system demonstrates an Ackermann steering car driving forward in Gazebo while following a yellow path painted on the ground plane. The car has a front-mounted camera sensor that publishes images from the vehicle's point of view. A Python ROS 2 node subscribes to this camera feed, thresholds the image for yellow, finds the centroid of the visible line, and converts the offset from image center into a normalized steering command.

That steering command is used in two places:

1. It publishes a ROS 2 `/cmd_vel` command to steer and move the simulated Ackermann car.
2. It sends the same steering value over UDP from WSL to Windows, where a C++ DirectInput controller converts it into force feedback commands for the MOZA wheel.

When the simulated line turns right, the simulated car turns right and the real wheel turns right. When the line turns left, the real wheel turns left. The final C++ controller uses target-angle PD control so the wheel does not spin endlessly. Instead, it moves toward a target steering angle and holds that angle more like a real steering wheel.

### Suggested Screenshots / Video Clips to Include

Add screenshots or clips showing:

* Gazebo world with the Ackermann car and yellow track
* The camera view or `/debug_camera/image` output
* The Python terminal showing detected yellow line position and steering values
* The Windows C++ controller showing received steering values and wheel control output
* The physical MOZA wheel moving while the Gazebo car follows the line

---

## Current System Architecture

```text
Gazebo Harmonic World
    ↓
Ackermann Car Front Camera
    ↓
Gazebo Camera Topic
    ↓
ros_gz_bridge
    ↓
ROS 2 Topic: /debug_camera/image
    ↓
yellow_line_tracking.py
    ├── OpenCV yellow-line detection
    ├── publishes /cmd_vel for simulated car
    └── sends UDP steering value to Windows
            ↓
Windows C++ DirectInput Controller
    ↓
MOZA R3 Force Feedback Wheel
```

---

## Repository Contents

Suggested repository structure:

```text
.
├── README.md
├── yellow_line_tracking.py
├── moza_udp_ffb.cpp
├── worlds
│   └── ackermann_adjusted.world
├── models
│   └── ackermann_car
│       └── model.sdf
└── media
    └── course.png
```

File descriptions:

```text
yellow_line_tracking.py
```

ROS 2 Python node that subscribes to the Gazebo camera image, detects the yellow line, publishes `/cmd_vel`, and sends UDP steering values to Windows.

```text
moza_udp_ffb.cpp
```

Windows C++ DirectInput program that receives UDP steering values and applies target-angle force feedback to the MOZA wheel.

```text
models/ackermann_car/model.sdf
```

Gazebo model for the Ackermann car, including the chassis, wheels, steering joints, Ackermann steering plugin, and front camera sensor.

```text
worlds/ackermann_adjusted.world
```

Gazebo world containing the ground plane, lighting, course texture, and car placement.

```text
media/course.png
```

Track image used as the ground texture. The yellow line is detected by the OpenCV node.

---

## Installation Instructions

These instructions assume a Windows 11 computer using WSL 2 with Ubuntu, ROS 2 Jazzy, Gazebo Harmonic, Visual Studio, and a MOZA R3 wheel.

### 1. Install WSL 2 and Ubuntu

On Windows PowerShell as Administrator:

```powershell
wsl --install
```

Restart if prompted. Then install or open Ubuntu from the Microsoft Store.

Update Ubuntu packages:

```bash
sudo apt update
sudo apt upgrade -y
```

---

### 2. Install ROS 2 Jazzy

Set locale:

```bash
sudo apt update
sudo apt install -y locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8
```

Enable required repositories:

```bash
sudo apt install -y software-properties-common
sudo add-apt-repository universe
sudo apt update
sudo apt install -y curl gnupg lsb-release
```

Add the ROS 2 package repository:

```bash
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | \
  sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
```

Install ROS 2 Jazzy:

```bash
sudo apt update
sudo apt install -y ros-jazzy-desktop
```

Source ROS 2:

```bash
source /opt/ros/jazzy/setup.bash
```

Optional: add this to `~/.bashrc`:

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

---

### 3. Install Gazebo Harmonic and ROS/Gazebo Bridge

Install Gazebo Harmonic and ROS/Gazebo bridge packages:

```bash
sudo apt update
sudo apt install -y gz-harmonic ros-jazzy-ros-gz
```

Verify Gazebo:

```bash
gz sim --version
```

---

### 4. Install Python Dependencies

Install Python packages needed by the ROS node:

```bash
sudo apt install -y python3-opencv python3-numpy ros-jazzy-cv-bridge
```

---

### 5. Install Windows C++ Build Tools

Install Visual Studio 2022 with:

* Desktop development with C++
* Windows 10 or Windows 11 SDK
* MSVC C++ build tools

The C++ project links against:

```text
dinput8.lib
dxguid.lib
ws2_32.lib
```

The source file also includes these pragmas, so Visual Studio should link them automatically:

```cpp
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ws2_32.lib")
```

---

### 6. Connect the MOZA Wheel

Before running the C++ wheel controller:

* Connect the MOZA R3 wheel and pedals to the Windows machine.
* Confirm Windows can see the wheel.
* Close MOZA Pit House before running the C++ controller.
* Run the C++ program as Administrator.

The C++ program uses Windows DirectInput and needs exclusive force feedback access to the wheel.

---

## How to Run the Code

The project requires several processes running at the same time.

### Terminal 1: Start Gazebo

From the project directory in WSL:

```bash
source /opt/ros/jazzy/setup.bash
export GZ_SIM_RESOURCE_PATH=$PWD/models:$PWD/worlds:$GZ_SIM_RESOURCE_PATH
gz sim worlds/ackermann_adjusted.world
```

Make sure the Gazebo simulation is unpaused.

---

### Terminal 2: Bridge the Camera Image from Gazebo to ROS 2

```bash
source /opt/ros/jazzy/setup.bash

ros2 run ros_gz_bridge parameter_bridge \
/debug_camera/image@sensor_msgs/msg/Image@gz.msgs.Image
```

Verify that ROS 2 receives image frames:

```bash
ros2 topic hz /debug_camera/image
```

---

### Terminal 3: Bridge `/cmd_vel` from ROS 2 to Gazebo

```bash
source /opt/ros/jazzy/setup.bash

ros2 run ros_gz_bridge parameter_bridge \
/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist
```

---

### Windows: Start the MOZA Wheel Controller

Build the C++ project in Visual Studio.

Then run the compiled `.exe` as Administrator.

Expected output includes:

```text
=== MOZA FFB UDP Steering Test ===
[Init] DirectInput initialized.
[Device] Force feedback supported.
[Init] Acquire ok.
[Test] UDP steering target-angle mode
[Test] Listening for steering values on UDP port 5005.
```

If the program fails with `Acquire failed: 0x80070005`, run it as Administrator and make sure MOZA Pit House is closed.

---

### Terminal 4: Run the Python Steering Node

Before running, update the Windows IP address in `yellow_line_tracking.py`:

```python
self.udp_ip = "YOUR_WINDOWS_IPV4_ADDRESS"
```

Find the Windows IP using PowerShell:

```powershell
ipconfig
```

Then run the Python node from WSL:

```bash
source /opt/ros/jazzy/setup.bash
python3 yellow_line_tracking.py
```

Expected Python output:

```text
yellow_cx=470 center=320 error=+0.469 steering=+0.562 speed=0.50
```

Expected C++ output:

```text
[CTRL] rawSteer=0.56 smoothSteer=0.52 targetDeg=62.40 logicalDeg=58.10 errorDeg=4.30 DIForce=-120
```

At this point:

* The Gazebo car should drive forward.
* The Gazebo car should follow the yellow line.
* The real MOZA wheel should turn in the same direction as the simulated vehicle.
* The wheel should hold an approximate steering angle instead of spinning forever.

---

## Important Parameters

### Python Parameters

In `yellow_line_tracking.py`:

```python
self.kp = 1.2
self.forward_speed = 0.5
self.max_turn_rate = 1.2
self.sim_steering_sign = -1.0
```

Parameter meanings:

```text
kp
```

Controls how aggressively the yellow line error is converted into steering.

```text
forward_speed
```

Controls the simulated car's forward speed.

```text
max_turn_rate
```

Controls the maximum angular command sent to the simulated car.

```text
sim_steering_sign
```

Used to flip the simulated car steering direction. In this project, `-1.0` was required.

---

### C++ Parameters

In `moza_udp_ffb.cpp`:

```cpp
constexpr double MAX_WHEEL_ANGLE_DEG = 120.0;
constexpr double KP = 18.0;
constexpr double KD = 0.45;
constexpr LONG MAX_FORCE = DI_FFNOMINALMAX / 3;
constexpr int DIRECTINPUT_FORCE_SIGN = -1;
constexpr int WHEEL_AXIS_SIGN = -1;
constexpr int PACKET_TIMEOUT_MS = 500;
```

Parameter meanings:

```text
MAX_WHEEL_ANGLE_DEG
```

Maximum target wheel angle in either direction.

```text
KP
```

Proportional gain for the wheel target-angle controller.

```text
KD
```

Damping gain to reduce oscillation.

```text
MAX_FORCE
```

Maximum DirectInput force command.

```text
DIRECTINPUT_FORCE_SIGN
```

Converts logical steering force to DirectInput signed force.

```text
WHEEL_AXIS_SIGN
```

Converts raw wheel axis position to logical wheel angle. This was important for preventing runaway full-lock behavior.

```text
PACKET_TIMEOUT_MS
```

If UDP packets stop arriving, the wheel target returns to center.

---

## Troubleshooting

### ROS topic exists, but `ros2 topic hz /debug_camera/image` shows nothing

A ROS subscriber can make a topic appear even if Gazebo is not publishing data into ROS. Check the Gazebo topic directly:

```bash
gz topic -l | grep debug_camera
```

Then make sure the camera bridge is running:

```bash
ros2 run ros_gz_bridge parameter_bridge \
/debug_camera/image@sensor_msgs/msg/Image@gz.msgs.Image
```

---

### UDP values are not reaching Windows

Do not send UDP to `127.0.0.1` from WSL if the receiver is on Windows. Use the Windows IPv4 address from `ipconfig`.

On Windows, a test receiver can bind to all interfaces:

```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5005))

print("Listening on UDP 5005...")

while True:
    data, addr = sock.recvfrom(1024)
    print(addr, data.decode())
```

---

### DirectInput compile errors with Winsock

Include order matters. `winsock2.h` must be included before `windows.h`:

```cpp
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <dinput.h>
```

---

### DirectInput `Acquire failed: 0x80070005`

This means access was denied when acquiring the wheel.

Fixes:

* Close MOZA Pit House.
* Close games or other tools using the wheel.
* Run the C++ program as Administrator.
* Click the DirectInput window if needed.

---

### Wheel turns all the way to one side

This is usually a sign mismatch in the target-angle controller.

Check:

```cpp
constexpr int WHEEL_AXIS_SIGN = -1;
```

If the wheel runs away to full lock, try flipping it:

```cpp
constexpr int WHEEL_AXIS_SIGN = 1;
```

The C++ debug output includes the current logical wheel angle. When the wheel physically turns right, logical angle should become positive. When the wheel physically turns left, logical angle should become negative.

---

## References

### Helpful References

* ROS 2 Jazzy documentation
  [https://docs.ros.org/en/jazzy/](https://docs.ros.org/en/jazzy/)

* Gazebo Harmonic documentation
  [https://gazebosim.org/docs/harmonic/](https://gazebosim.org/docs/harmonic/)

* ROS/Gazebo bridge documentation
  [https://github.com/gazebosim/ros_gz](https://github.com/gazebosim/ros_gz)

* Gazebo Sensors system documentation
  [https://gazebosim.org/api/sim/](https://gazebosim.org/api/sim/)

* OpenCV color conversion and HSV thresholding documentation
  [https://docs.opencv.org/](https://docs.opencv.org/)

* Microsoft DirectInput documentation
  [https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416842(v=vs.85)](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416842%28v=vs.85%29)

* Microsoft Winsock documentation
  [https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page-2](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page-2)

* MOZA Racing official site
  [https://mozaracing.com/](https://mozaracing.com/)

### References / Approaches That Were Less Helpful

* Generic DirectInput force feedback examples were only partially useful because many examples assume older joysticks or gamepads rather than modern racing wheel bases.
* DirectInput direction-vector examples were less useful for this wheel. The working behavior used signed constant-force magnitude instead.
* General ROS/Gazebo tutorials were useful for setup, but many did not cover the exact Gazebo Harmonic + ROS 2 Jazzy + WSL + Windows hardware-in-the-loop workflow used here.
* Generic WSL networking examples were somewhat confusing because `127.0.0.1` from WSL did not work for sending UDP to the Windows receiver. Using the Windows IPv4 address from `ipconfig` worked.

---

## Future Work

### 1. Improve Course Complexity

The current course demonstrates line following and left/right steering, but more track layouts could be added. Future courses could include:

* tighter turns
* intersections
* dashed lines
* variable-width curves
* multiple colored paths
* obstacles or road boundaries

This would make the computer vision and control problem more realistic.

---

### 2. Improve Computer Vision Robustness

The current OpenCV logic uses a simple HSV threshold for yellow. This works well for the current Gazebo track, but it is sensitive to color, lighting, and camera angle.

Future improvements:

* Add dynamic threshold tuning.
* Add morphological filtering controls.
* Use contour filtering by area.
* Ignore yellow detections that are too small or too high in the image.
* Add debug image output showing the mask and detected centroid.
* Use a lookahead point instead of the full yellow mask centroid.

A lookahead-based method would likely improve path following because the car would steer based on the path farther ahead, not just the average of all visible yellow pixels.

---

### 3. Improve Vehicle Control

The current simulated car control is proportional line following. It works, but may oscillate at higher speeds.

Future improvements:

* Add PID steering control.
* Add speed control based on curve sharpness.
* Slow down before tight turns.
* Tune steering gain based on velocity.
* Use pure pursuit or Stanley controller style path tracking.

---

### 4. Improve Wheel Force Feedback Realism

The final wheel controller uses target-angle PD control, which is much better than raw continuous force control. However, it is still a simplified force feedback model.

Future improvements:

* Read more accurate wheel angle data if available.
* Add soft end stops.
* Add self-centering force that varies with simulated speed.
* Add road texture or vibration effects.
* Add configurable steering ratio.
* Add a small GUI for tuning `KP`, `KD`, max angle, and max force live.

---

### 5. Combine Bridge Commands into a Launch Script

Currently, multiple terminals are used:

* Gazebo
* image bridge
* `/cmd_vel` bridge
* Python node
* Windows C++ wheel controller

Future work could add scripts to launch the WSL side with one command.

Example future script:

```bash
./run_sim.sh
```

Possible contents:

```bash
#!/usr/bin/env bash
set -e

source /opt/ros/jazzy/setup.bash
export GZ_SIM_RESOURCE_PATH=$PWD/models:$PWD/worlds:$GZ_SIM_RESOURCE_PATH

gz sim worlds/ackermann_adjusted.world &
ros2 run ros_gz_bridge parameter_bridge /debug_camera/image@sensor_msgs/msg/Image@gz.msgs.Image &
ros2 run ros_gz_bridge parameter_bridge /cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist &
python3 yellow_line_tracking.py
```

---

### 6. Configuration File Support

Several important values are currently hard-coded, including:

* Windows UDP IP address
* UDP port
* camera topic
* `/cmd_vel` topic
* forward speed
* steering gain
* C++ force feedback gains

Future work could move these into a config file such as:

```text
config.yaml
```

This would make the project easier to reuse on other machines.

---

### 7. Better Cross-Platform Networking Setup

The UDP connection currently requires manually setting the Windows IPv4 address in Python. A more robust approach would:

* automatically detect the Windows host IP from WSL
* allow command-line override
* print a clear startup warning if packets fail
* include a heartbeat message between Python and C++

---

### 8. Package the ROS Node Properly

The Python file is currently run directly. Future work could turn it into a formal ROS 2 package with:

* `package.xml`
* `setup.py`
* launch files
* parameters
* proper install rules

This would make it easier to run with standard ROS 2 commands.

---

## Project Status

The final project successfully demonstrates a hardware-in-the-loop Gazebo driving demo:

* Gazebo car camera publishes image data.
* ROS 2 receives the image stream.
* Python/OpenCV detects a yellow line.
* The simulated car follows the line.
* Steering commands are sent from WSL to Windows over UDP.
* A Windows C++ DirectInput controller moves the physical MOZA wheel.
* The wheel uses target-angle PD control for more realistic behavior.

The main remaining improvements are polish, easier launch automation, better vision robustness, and more advanced vehicle/wheel control models.
