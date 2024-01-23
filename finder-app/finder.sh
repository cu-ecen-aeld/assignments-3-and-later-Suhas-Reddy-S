#!/bin/sh
#Author: Suhas-Reddy-S

filesdir=$1
searchstr=$2

if [ "$#" -ne 2 ]
then
  echo "Total Number of arguements should be equal to two. First arguement is the directory path and Second arguement is the String to be searched."
  exit 1
else 
  if [ ! -d "${filesdir}" ] 
  then
    echo "Given ${filesdir} driectory doesn't exist in the filesystem. Please enter a valid path."
    exit 1
  else 
    num_of_files=$(ls -R ${filesdir} | wc -l)
    num_of_occurances=$(grep -R ${searchstr} ${filesdir} | wc -l)
    echo "The number of files are ${num_of_files} and the number of occurances are ${num_of_occurances}."
  fi
fi

