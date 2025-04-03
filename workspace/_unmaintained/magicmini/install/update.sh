#!/bin/sh

# becomes /.system/magicmini/bin/install.sh

# clean up from an previous ill-considered update
DTB_PATH=/storage/TF2/.system/magicmini/dat/rk3562-magicx-linux.dtb
if [ -f "$DTB_PATH" ]; then
	rm -rf "$DTB_PATH"
fi

