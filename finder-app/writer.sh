#!/bin/sh
# Author: Suhas-Reddy-S

# Variables to hold first and second CLI arguments
writefile=$1 
writestr=$2

# Check if the number of arguments is not equal to 2
if [ "$#" -ne 2 ] 
then
  echo "Total number of arguments should be equal to two. The first argument is the file path, and the second argument is the string to be added to the file."
  exit 1
else
  # Create the directory (if not exists) for the file
  mkdir -p "$(dirname "${writefile}")"

  # Write the string to the file
  echo "${writestr}" > "${writefile}"

  # Check if the file creation was successful
  if [ $? -ne 0 ] 
  then
    echo "Error creating the file"
    exit 1
  fi
fi
