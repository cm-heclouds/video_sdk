ifconfig eth0 192.168.200.151
sleep 3
route add default gw 192.168.200.1
cd /mnt/mtd/
/mnt/mtd/sample_test &

