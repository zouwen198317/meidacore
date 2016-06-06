rm mediacore > /dev/null 2>&1
make -j4

if [ -f mediacore ]; then
	chmod 777 mediacore >/dev/null 2>&1
	cp mediacore /home/adas/share/ >/dev/null 2>&1
	mv mediacore /mnt/hgfs/share >/dev/null 2>&1

	if [ $? -eq 0 ]; then
		echo "chmod 777 and move to /home/adas/share/"
	fi
fi
