The instruction for flashing not OTA firmware over another laptop

Type sudo apt-get install openssh-server
Enable the SSH service by typing sudo systemctl enable ssh
Start the SSH service by typing sudo systemctl start ssh
Connect to laptop via SSH
Install esptool by typing sudo apt install esptool
Find out IP address by typing ip addr
Open *.bin file folder
ll /sys/class/tty/ttyUSB*
minicom
For start flashing type sudo esptool -cd nodemcu -cf firmware.bin