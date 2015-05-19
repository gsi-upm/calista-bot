#!/bin/bash

FILENAME=ChatScript-5.31.zip
TMPFOLDER=INSTALL
BASEFOLDER=`pwd`

# CREATES TMP DIR
echo "Creting tempfolder $TMPFOLDER"
mkdir -p $TMPFOLDER
cd $TMPFOLDER

echo -n "Getting ChatScript..."
wget -O $FILENAME http://downloads.sourceforge.net/project/chatscript/ChatScript-5.31.zip >wget.log 2>&1
cd $BASEFOLDER
unzip $TMPFOLDER/$FILENAME
chmod +x LinuxChatScript64 LinuxChatScript32
echo " done!"

echo  -n "Cleaning up..."
rm -rf $TMPFOLDER
echo " done!"
