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
fi
# Extract the directory path from the provided file path
dir_path="$(dirname "${writefile}")"

# Check if the directory already exists
if [ ! -d "$dir_path" ] 
then 
  # Create the directory if it doesn't exist
  mkdir -p "$dir_path"
fi

# Check if the specified path is a directory
if [ -d "$writefile" ] 
then
  echo "Error: The specified path is a directory. Please provide a valid file path."
  exit 1
else
  # Write the string to the file
  echo "${writestr}" > "${writefile}"
  exit 0
fi

# Check if the file creation was successful
if [ $? -ne 0 ] 
then
  echo "Error creating the file"
fi
