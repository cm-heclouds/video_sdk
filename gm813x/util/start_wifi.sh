cd /lib/modules/ 
insmod mt7601Usta.ko
/lib/modules/wpa_passphrase onenetvideotest "12345678" >> /usr/wpa_supplicant.conf 
sleep 2s 
/lib/modules/wpa_supplicant -Dwext -ira0 -c/usr/wpa_supplicant.conf -B
sleep 3s
ifconfig ra0 `udhcpc -i  ra0 | grep 'Lease of'| awk '{print $3}'`
sleep 1s
route add default gw `ifconfig|grep 'inet addr'|grep -v '127.0.0.1'|cut -d: -f2 |awk '{print $1}'| awk '{split($0,ip,"." ); printf "%s.%s.%s.1\n",ip[1],ip[2],ip[3] }'`

cd /mnt/mtd

./sample_test  &
