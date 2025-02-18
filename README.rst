.. zephyr:code-sample:: ble_central
   :name: Central
   :relevant-api: bluetooth

   Implement a Beacon and a Central role (with filter) using Bluetooth LE without connection.

Overview
********

Application demonstrating very basic Bluetooth LE Central role functionality by scanning
for other Bluetooth LE devices by filter their MAC address.
Also, it advertises itself as a Beacon device.

Requirements
************

* BlueZ running on the host, or
* A board with Bluetooth LE support

Building and Running
********************
This sample can be found under :zephyr_file:`samples/bluetooth/central` in the
Zephyr tree.

See :ref:`bluetooth samples section <bluetooth-samples>` for details.
