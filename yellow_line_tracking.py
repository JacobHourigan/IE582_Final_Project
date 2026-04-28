#!/usr/bin/env python3

import socket

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge


def clamp(value, low, high):
    return max(low, min(high, value))


class YellowLineSteering(Node):
    def __init__(self):
        super().__init__("yellow_line_steering")

        self.bridge = CvBridge()

        self.subscription = self.create_subscription(
            Image,
            "/debug_camera/image",
            self.image_callback,
            10,
        )

        # Line-following gain
        self.kp = 1.2

        # UDP output to Windows wheel controller
        self.udp_ip = "172.24.208.1"
        self.udp_port = 5005
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self.get_logger().info("Yellow line steering node started.")
        self.get_logger().info("Listening on /debug_camera/image")
        self.get_logger().info(
            f"Sending steering UDP to {self.udp_ip}:{self.udp_port}"
        )

    def send_steering(self, steering):
        steering = clamp(float(steering), -1.0, 1.0)
        msg = f"{steering:.4f}".encode("utf-8")
        self.sock.sendto(msg, (self.udp_ip, self.udp_port))

    def image_callback(self, msg):
        frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding="bgr8")

        height, width, _ = frame.shape

        # Use lower half of image only
        roi = frame[height // 2 : height, :]

        hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)

        # Yellow HSV threshold
        lower_yellow = np.array([20, 80, 80])
        upper_yellow = np.array([35, 255, 255])

        mask = cv2.inRange(hsv, lower_yellow, upper_yellow)

        # Clean up noise
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.erode(mask, kernel, iterations=1)
        mask = cv2.dilate(mask, kernel, iterations=2)

        moments = cv2.moments(mask)

        if moments["m00"] == 0:
            steering = 0.0
            self.send_steering(steering)
            self.get_logger().info("No yellow line detected | steering=0.000")
            return

        cx = int(moments["m10"] / moments["m00"])

        image_center = width // 2
        error_pixels = cx - image_center
        error_normalized = error_pixels / image_center

        # Positive means line is to the right, negative means left
        steering = clamp(self.kp * error_normalized, -1.0, 1.0)

        self.send_steering(steering)

        self.get_logger().info(
            f"yellow_cx={cx:4d} center={image_center:4d} "
            f"error={error_normalized:+.3f} steering={steering:+.3f}"
        )


def main(args=None):
    rclpy.init(args=args)
    node = YellowLineSteering()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.send_steering(0.0)
        node.sock.close()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()