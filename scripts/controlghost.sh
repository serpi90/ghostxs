#!/bin/bash

BOTS_ROOT=/home/user/ghostxs/
VERBOSE=1

# Warn if not enough parameters
if [ "$#" -lt 2 ]
then
	echo "Usage: $0 [BOTNAME] start|stop|stopforce|kill|restart|status"
	exit 1
fi

# Enable verbose on -v or --verbose
for each in `echo $@ |  grep -oE "\-[a-Z-]*\b"`
do
        if [ $each = "-v" ] || [ $each = "--verbose" ]
        then
                VERBOSE=1
        fi
done

BOTNAME=$1
OPERATION=$2

######################
## Helper functions ##
######################

function checkValidBot
{
	if [ -d $BOTS_ROOT/accounts/$BOTNAME ] && [ $BOTNAME != example ]
	then
		RESULT=1
	else
		RESULT=0
	fi
}

function getStatus
{
	RESULT=`ps -ewwo args | grep -c [G]$BOTNAME`
}

function getPid
{
	local PARENT_PID
	PARENT_PID=`ps -ewwo pid,args | grep [G]$BOTNAME | sed -r 's/^\ *([0-9]*).*/\1/'`
	RESULT=`ps -ewwo pid,ppid,args | grep -E "^\ *[0-9]+\ +$PARENT_PID" | sed -r 's/^\ *([0-9]*).*/\1/'`
}

function stopBot
{
	getStatus
	if [ $RESULT != 0 ]
	then
		getPid
		kill -s SIGINT $RESULT
	fi
}

function stopForceBot
{
	stopBot
	stopBot
}

function killBot
{
	getStatus
	if [ $RESULT != 0 ]
	then
		getPid
		kill -s SIGKILL $RESULT
	fi
}

function startBot
{
	if [ -d $BOTS_ROOT/accounts/$BOTNAME/ ] && [ $BOTNAME != example ]
	then
		getStatus
		if [ $RESULT = 0 ]
		then
			cd $BOTS_ROOT/accounts/$BOTNAME/
			screen -dmS G$BOTNAME ./ghost++
		fi
	fi
}

function botNotRunning
{
	echo Error: $BOTNAME is not running.
	exit 1
}

#################
## Core script ##
#################

checkValidBot
if [ $RESULT = 0 ]
then
	echo $BOTNAME is not recognised as a valid bot name/folder
	exit 1
fi

if [ $OPERATION = start ]
then
	getStatus
	if [ $RESULT != 0 ]
	then
		echo Error: $BOTNAME is already started.
		exit 1
	else
		startBot
		if [ $VERBOSE = 1 ]; then echo Started $BOTNAME; fi
	fi
elif [ $OPERATION = stop ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopBot
		if [ $VERBOSE = 1 ]; then echo Stopped $BOTNAME; fi
	fi
elif [ $OPERATION = stopforce ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopForceBot
		if [ $VERBOSE = 1 ]; then echo Stopped forcefully $BOTNAME; fi
	fi
elif [ $OPERATION = kill ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		killBot
		if [ $VERBOSE = 1 ]; then echo Killed process for $BOTNAME; fi
	fi
elif [ $OPERATION = restart ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopBot
		if [ $VERBOSE = 1 ]; then echo Stopped $BOTNAME; fi
		sleep 2
		startBot
		if [ $VERBOSE = 1 ]; then echo Started $BOTNAME; fi
	fi
elif [ $OPERATION = status ]
then
	getStatus
	if [ $RESULT = 0 ]
	then echo $BOTNAME is not Running.
	else echo $BOTNAME is Running.
	fi
else
	echo ERROR: $OPERATION is not recognised as a valid operand.
fi

