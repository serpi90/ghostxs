#!/bin/bash

BOTS_ROOT=/home/$USER/lghostxs/
VERBOSE=0

# Show help and exit
if [ $(echo $@ | grep -ioE "\-[a-z-]*\b" | grep -cE '(-h)|(--help)') -gt 0 ]
then
	echo "Usage: $0 [OPERATION] [BOTNAME] -v --verbose -h --help"
	echo "Operations:"
	echo " - create		create a new bot instance, in accounts folder"
	echo " - list		list current bots, in accounts folder"
	echo " - kill		terminate the bot with a kill signal"
	echo " - status		check if the bot is running"
	echo " - start		start the bot"
	echo " - stop		send a nice stop, will wait for current games to end"
	echo " - stopforce	equivalent to sending 2 times stop, stops the current games and finishes"
	echo " - restart	send a stopforce and then start the bot"
	exit 1
fi

# Warn if not enough parameters
if [ "$#" -lt 1 ]
then
	echo "Usage: $0 [OPERATION] [BOTNAME] -v --verbose -h --help"
	echo "Operations:"
	echo "create, kill, status, start, stop, stopforce, restart"
	exit 1
fi

# Enable verbose on -v or --verbose
for each in $(echo $@ |  grep -oE "\-[a-Z-]*\b")
do
        if [ $each = "-v" ] || [ $each = "--verbose" ]
        then
			VERBOSE=1
        fi
done

OPERATION=$1
BOTNAME=$2

######################
## Helper functions ##
######################

vecho ()
{
	if [ $VERBOSE -eq 1 ]
	then
		echo $@
	fi
}

checkValidBot ()
{
	if [ -d $BOTS_ROOT/accounts/$BOTNAME ] && [ q$BOTNAME != "qexample" ]
	then
		RESULT=1
	else
		RESULT=0
	fi
}

listBots ()
{
	for bot in $(ls $BOTS_ROOT/accounts/ -F | grep -ioE '\b\w*')
	do
		if [ $bot != "example" ]
		then
			echo -n "$bot "
		fi
	done
	echo
}

getStatus ()
{
	RESULT=`ps -ewwo args | grep -c [G]$BOTNAME`
}

getPid ()
{
	local PARENT_PID
	PARENT_PID=`ps -ewwo pid,args | grep [G]$BOTNAME | sed -r 's/^\ *([0-9]*).*/\1/'`
	RESULT=`ps -ewwo pid,ppid,args | grep -E "^\ *[0-9]+\ +$PARENT_PID" | sed -r 's/^\ *([0-9]*).*/\1/'`
}

stopBot ()
{
	getStatus
	if [ $RESULT != 0 ]
	then
		getPid
		kill -s SIGINT $RESULT
	fi
}

stopForceBot ()
{
	stopBot
	stopBot
}

killBot ()
{
	getStatus
	if [ $RESULT != 0 ]
	then
		getPid
		kill -s SIGKILL $RESULT
	fi
}

startBot ()
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

botNotRunning ()
{
	echo Error: $BOTNAME is not running.
	exit 1
}

create ()
{
	for folder in $(ls $BOTS_ROOT/accounts/)
	do
		if [ $folder == $BOTNAME ]
		then
			echo Error: Bot $BOTNAME exists, not creating.
			exit 1
		fi
	done
	mkdir -p $BOTS_ROOT/accounts/$BOTNAME/
	cp -r $BOTS_ROOT/accounts/example/* $BOTS_ROOT/accounts/$BOTNAME/
	vecho Created $BOTNAME folder in $BOTS_ROOT/accounts/$BOTNAME please modify ghost.cfg
}

#################
## Core script ##
#################

checkValidBot
if [ $OPERATION = create ]
then
	create
	exit 1
elif [ $RESULT = 0 ]
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
		vecho Started $BOTNAME
	fi
elif [ $OPERATION = stop ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopBot
		vecho Stopped $BOTNAME
	fi
elif [ $OPERATION = stopforce ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopForceBot
		vecho Stopped forcefully $BOTNAME
	fi
elif [ $OPERATION = kill ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		killBot
		vecho Killed process for $BOTNAME
	fi
elif [ $OPERATION = restart ]
then
	getStatus
	if [ $RESULT = 0 ]
	then botNotRunning
	else
		stopForceBot
		vecho Stopped $BOTNAME
		sleep 2
		startBot
		vecho Started $BOTNAME
	fi
elif [ $OPERATION = status ]
then
	getStatus
	if [ $RESULT = 0 ]
	then echo $BOTNAME is not Running.
	else echo $BOTNAME is Running.
	fi
elif [ $OPERATION = list ]
then
	listBots
else
	echo ERROR: $OPERATION is not recognised as a valid operand.
fi

