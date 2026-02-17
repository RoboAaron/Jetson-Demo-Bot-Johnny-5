"""
Backward-compatibility shim.
The node was moved into the balance_bridge package.
Use:  ros2 run balance_bridge balance_bridge_node
  or: ros2 launch balance_bridge balance_bridge.launch.py
"""
from balance_bridge.balance_bridge_node import main  # noqa: F401

if __name__ == '__main__':
    main()
