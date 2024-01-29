#!/bin/sh
# Author: Suhas-Reddy-S

# Variables to hold first and second CLI arguments
filesdir=$1
searchstr=$2

# Check if there are 2 arguments
if [ "$#" -ne 2 ] 
then
  echo "Total number of arguments should be equal to two. First argument is the directory path and the second argument is the string to be searched."
  exit 1
fi 
# Check if the first argument is a directory; if not, print an error message
if [ ! -d "${filesdir}" ] 
then    
  echo "Error: The specified directory '${filesdir}' doesn't exist. Please enter a valid path."
  exit 1
fi 
# Get the number of files in the directory
num_of_files=$(ls -r "${filesdir}" | wc -l)

# Get the number of occurrences of the search string
num_of_occurrences=$(grep -r "${searchstr}" "${filesdir}" | wc -l)

echo "The number of files are  ${num_of_files} and the number of matching lines are ${num_of_occurrences}."
