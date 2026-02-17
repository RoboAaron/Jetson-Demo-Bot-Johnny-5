# Johnny 5 — TODO
_Last updated: 2025-02-11_

## Mechanical

- [ ] **Create drill template for final aluminum base plates** — Transfer mounting hole positions from current prototype (wood/test plates) to a measured drill layout for the 3× 8×8" aluminum plates. Include holes for: motor mount brackets, standoff posts, Jetson mount, FSESC mount, Teensy breakout, battery tie-downs, mast bracket, and any cable pass-throughs. Consider using Onshape/Zoo.ai CAD to produce a printable 1:1 template.
- [ ] Measure actual `wheel_separation` (axle center-to-center) with calipers/tape — currently estimated at ~0.30m [TENTATIVE]
- [ ] Measure actual chassis stack height (bottom plate to top plate) — currently estimated at ~0.28m (user reports 10-12")
- [ ] Measure wheel width with calipers — currently estimated at 0.055m
- [ ] Verify mast bracket x/y offset from chassis center
- [ ] Decide LiDAR mounting position (micro-deck vs mast mount vs chassis-edge)
- [ ] Measure IMU (BNO085) position relative to wheel axle center

## Software

- [ ] Implement Teensy firmware with micro-ROS (CAN bridge + IMU publisher)
- [ ] Implement balance controller on Teensy (PID, BNO085 pitch feedback)
- [ ] Test Gazebo simulation with stabilizer casters (verify robot stays upright)
- [ ] Tune EKF process noise covariance on real hardware
- [ ] Resolve TF conflicts between old `complete_lidar_slam.launch.py` static TFs and new URDF-based TFs
