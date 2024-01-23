#!/bin/sh
#Author: Suhas-Reddy-S

#Variables to hold fisrt and second cli arguements
writefile=$1 
writestr=$2

if [ "$#" -ne 2 ]
then
  echo "Total Number of arguements should be equal to two. First arguement is the file path and Second arguement is the String to be added to the file."
  exit 1
else
  mkdir -p "$(dirname "${writefile}")"
  echo "${writestr}" > "${writefile}"
  if [ $? -ne 0 ]
  then
    echo "Error creating a file"
    exit 1
  fi
fi

