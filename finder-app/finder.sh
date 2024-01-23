#!/bin/sh
#Author: Suhas-Reddy-S

#Variables to hold fisrt and second cli arguements
filesdir=$1
searchstr=$2

#Check if there are 2 arguements
if [ "$#" -ne 2 ]
then
  echo "Total Number of arguements should be equal to two. First arguement is the directory path and Second arguement is the String to be searched."
  exit 1
else 
  #Check if the first arguement is directory else print an error message.
  if [ ! -d "${filesdir}" ] 
  then    
    echo "Given ${filesdir} driectory doesn't exist in the filesystem. Please enter a valid path."
    exit 1
  else 
    num_of_files=$(ls ${filesdir} | wc -l)   #Get number of files
    num_of_occurances=$(grep -r ${searchstr} ${filesdir} | wc -l) #Get number of occurances
    echo "The number of files are ${num_of_files} and the number of occurances are ${num_of_occurances}."
  fi
fi

