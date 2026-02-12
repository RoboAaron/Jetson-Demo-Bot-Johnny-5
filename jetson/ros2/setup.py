from setuptools import find_packages, setup
import os

package_name = 'balance_bridge'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        # ament resource index marker
        ('share/ament_index/resource_index/packages',
         ['resource/' + package_name]),
        # package.xml
        ('share/' + package_name, ['package.xml']),
        # launch files
        (os.path.join('share', package_name, 'launch'),
         ['launch/balance_bridge.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Johnny-5 Team',
    maintainer_email='robot@example.com',
    description='ROS 2 bridge for the Teensy balance controller',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'balance_bridge_node = balance_bridge.balance_bridge_node:main',
        ],
    },
)
