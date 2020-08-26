#!/bin/sh

dd if=/dev/zero of=BlankUEFI.img bs=512 count=131072

sfdisk BlankUEFI.img << EOF
	label: gpt
   	unit: sectors
	first-lba: 34
	last-lba: 131038
	file.img1: start=2048, size=128991, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="EFI System"
EOF