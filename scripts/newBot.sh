#!\bin\bash
if [ $# -lt 1 ]
then
	echo "Usage: $0 [botName]" >&2
	echo "Will create a new folder in accounts, named 'botName'" >&2
	exit 1
fi

BOTS_ROOT=/home/user/ghostxs/

cd $BOTS_ROOT/accounts/

CURRENT_FOLDERS=`ls`
for folder in $CURRENT_FOLDERS
do
	if [ $folder = $1 ]
	then
		echo "Ya existe una carpeta con ese nombre."
		exit
	fi
done

mkdir $1/
cd $1

cp -Pr ../example/* ./
