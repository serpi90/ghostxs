#!/bin/bash

BOTS_ROOT=/home/$USER/ghostxs/

MAPSDIR=$BOTS_ROOT/shared/maps/
MAPCFGDIR=$BOTS_ROOT/shared/mapcfgs/

MAPNAME=`wget -q -O - getdota.com | grep file_name | grep -m 1 -o "DotA v[a-z\.0-9]*\.w3x"`

# Maps are stored in a subfolder called noMap, to prevent the use of .map, enforcing .load

mkdir -p $MAPSDIR/noMap

DOWNLOADED=`ls -l $MAPSDIR/noMap | grep -c "$MAPNAME"`

if [ $DOWNLOADED = 0 ]
then
	echo Downloading $MAPNAME $(date)
	wget -q "http://media.playdota.com/maps/eng/$MAPNAME" -O "$MAPSDIR/noMap/$MAPNAME"
	sed -r "s/DotA v[a-z\.0-9]*w3x/$MAPNAME/" $MAPCFGDIR/dota.cfg > /tmp/dota.cfg
	cat /tmp/dota.cfg > $MAPCFGDIR/dota.cfg
fi
