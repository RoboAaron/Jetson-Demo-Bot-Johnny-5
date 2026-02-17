"""
ament/colcon test entry-point for the balance_bridge package.

This file is discovered by 'colcon test' and re-runs the unit tests that live
in jetson/tests/ so they are also exercised during a ROS 2 CI build.
"""

import os
import sys
import pytest

# Ensure jetson/ and tuning_code/ are on the path when colcon runs these.
_here    = os.path.dirname(os.path.abspath(__file__))
_ros2    = os.path.dirname(_here)                    # jetson/ros2/
_jetson  = os.path.dirname(_ros2)                    # jetson/
_repo    = os.path.dirname(_jetson)                  # repo root
for _p in [_jetson, os.path.join(_repo, 'tuning_code')]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

# Delegate to the canonical test file
from jetson.tests.test_watchdog import (              # noqa: F401
    TestWatchdog, TestHalt, TestSetVelocity, TestReconnect,
)
