#!/bin/bash
if [[ "$1" == "" || "$2" == "" ]]; then
  echo "missing arguments!"
  echo "First arg is the (abs or rel) path to the exec."
  echo "Second arg is the (abs or rel) path to the folder with .dat files in it"
  exit 0;
fi

CMD=$(readlink -f $1)
DIR=$(readlink -f $2)
FILES=$DIR/*.dat
COUNT=0

#temporarily change to working dir
pushd $DIR
if [ -d converted ]; then
  read -p "deleting existing 'converted' folder... ok? (Y/n)" CONT
    if [ "$CONT" = "n" ]; then
      echo "goodbye";
      exit -1;
    else
      rm -rf converted;
    fi
fi

#create new folder for converted files
echo "Creating new 'converted' folder"
mkdir converted

#call previously made command
for f in $FILES
do
	$CMD $f null $DIR/converted/"$(basename $f .dat).es"
  COUNT=$[$COUNT+1]
done

echo "A total of $COUNT files converted. Have a nice day"

#change back to dir where we came from
popd
