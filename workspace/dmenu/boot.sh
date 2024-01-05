# intentionally omit shebang because plus uses /bin/sh and non-plus uses /system/bin/sh

# original rg35xx was a very strange environment
PLATFORM=rg35xx
CUT='busybox cut'
TAR='busybox tar'
GREP='busybox grep'
TAIL='busybox tail'

# the plus is more sensible (but still strange!)
if [ -f /etc/os-release ]; then
	PLATFORM=rg35xxplus
	CUT=cut
	TAR=tar
	GREP=grep
	TAIL=tail
fi

LINE=$((`$GREP -na '^PAYLOAD' $0 | $CUT -d ':' -f 1 | $TAIL -1` + 1))
$TAIL -n +$LINE "$0" > /tmp/data
$TAR -xf /tmp/data > /dev/null 2>&1
mv boot-$PLATFORM.sh $0
rm boot-rg35xx*
rm /tmp/data

./$0

exit 0
