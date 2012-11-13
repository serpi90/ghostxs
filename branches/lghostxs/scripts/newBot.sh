#!\bin\bash

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

cp -P ../example/* ./
