#!/bin/bash

BOTSDIR=/home/user/ghostxs/
SHAREDBOTDIR=$BOTSDIR/shared/

MAPNAME=`wget -q -O - getdota.com | grep file_name | grep -m 1 -o "DotA v[a-z\.0-9]*\.w3x"`

DOWNLOADED=`ls -l $SHAREDBOTDIR/maps | grep -c "$MAPNAME"`

if [ $DOWNLOADED = 0 ]
then
	echo Downloading $MAPNAME `date`
	wget "http://media.playdota.com/maps/eng/$MAPNAME" -O "$SHAREDBOTDIR/maps/$MAPNAME"
	sed -r "s/DotA v[a-z\.0-9]*w3x/$MAPNAME/" $SHAREDBOTDIR/mapcfgs/dota.cfg > /tmp/dota.cfg
	cat /tmp/dota.cfg > $SHAREDBOTDIR/mapcfgs/dota.cfg
fi
