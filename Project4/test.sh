#!/bin/bash

#_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ #
##       ______ _    _ _   _  _____ _______ _____ ____  _   _  _____          ##
###      |  ____| |  | | \ | |/ ____|__   __|_   _/ __ \| \ | |/ ____|       ###
####     | |__  | |  | |  \| | |       | |    | || |  | |  \| | (___        ####
####     |  __| | |  | | . ` | |       | |    | || |  | | . ` |\___ \       ####
###      | |    | |__| | |\  | |____   | |   _| || |__| | |\  |____) |       ###
##       |_|     \____/|_| \_|\_____|  |_|  |_____\____/|_| \_|_____/         ##
#------------------------------------------------------------------------------#

# Function to echo a string param in green. Does not return newline.
text_to_green() {
  text=$1
  echo -ne "\e[32m$1\e[39m"
}

# Function to echo a string param in red. Does not return newline.
text_to_red() {
  text=$1
  echo -ne "\e[31m$1\e[39m"
}

# Function to echo a string param in yellow. Does not return newline.
text_to_yellow() {
  text=$1
  echo -ne "\e[33m$1\e[39m"
}

# Function called whenever a test is passed. Increments num_tests_passed
pass() {
  text_to_green "PASS"
  echo
  let "num_tests_passed++"
}

# Function called whenever a test is failed.
fail() {
  text_to_red "FAIL"
  echo
  if [ "$ignore" = false ]; then exit 1; fi
}

# For fragmented files.
fluke() {
  text_to_red "FAIL"
  echo
  echo
  text_to_green "Even though this test failed, we don't need to implement "
  echo
  text_to_green "fragmented files for this project. If you did, congrats. "
  echo
  text_to_green "Continuing..."
  echo
}

# Function called when command should raise some FUSE error.
expect_error() {
  command=$1
  echo -ne " > \e[33m"
  $1
  echo -ne "\e[39m"
}

# How many tests did you pass? You can fail the fluke test and still be fine.
summary() {
  echo
  echo "Passed ${num_tests_passed}/13 tests. (Only 12 passes required)"
  echo
}

# Function to reset the disk so that we can test large files and whatnot.
clear_disk() {
  echo
  echo "Clearing the .disk file..."
  echo
  dd bs=1k count=5k if=/dev/zero of=.disk
  echo
}

#_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ #
##                        __  __          _____ _   _                         ##
###                      |  \/  |   /\   |_   _| \ | |                       ###
####                     | \  / |  /  \    | | |  \| |                      ####
####                     | |\/| | / /\ \   | | | . ` |                      ####
###                      | |  | |/ ____ \ _| |_| |\  |                       ###
##                       |_|  |_/_/    \_\_____|_| \_|                        ##
#------------------------------------------------------------------------------#

# This is the mount directory, should probably just be 'testmount'.
MOUNT=$1

# For ignoring failed tests
FLAG=$2
ignore=false;


if [ "$FLAG" = "-i" ]; then $ignore=true; fi

# Track number of tests that pass.
declare -i num_tests_passed=0

# Always start with an empty .disk file.
clear_disk

# Make sure the mount directory exists.
if [ -d "${MOUNT}" ]
then
  echo -e "Testing cs1550.c in ${MOUNT}\e[39m";
else
  echo "${MOUNT} does not exist or is not a directory."
  exit 1
fi

####-------- TEST 1 ---- TEST 1 ---- TEST 1 ---- TEST 1 ---- TEST 1 --------####

# Try to call mkdir() to make a directory.
echo
echo -ne " > Test 1: making a single directory... "
mkdir ${MOUNT}/dir_1

# Did we make a directory?
if [ -d "${MOUNT}/dir_1" ]; then pass; else fail; fi

####-------- TEST 2 ---- TEST 2 ---- TEST 2 ---- TEST 2 ---- TEST 2 --------####

# Try to call mkdir() on a directory name that's already in use.
echo
echo " > Test 2: making a duplicate directory..."
expect_error "mkdir ${MOUNT}/dir_1"

# Get the number of directories that exist within the root.
num_dirs=$(find ${MOUNT}/* -maxdepth 0 -type d | wc -l)
echo -n " > "

# Do we have only 1 directory?
if [ ${num_dirs} -eq 1 ]; then pass; else fail; fi

####-------- TEST 3 ---- TEST 3 ---- TEST 3 ---- TEST 3 ---- TEST 3 --------####

# Test 3: Directory name > 8 characters.
echo
echo " > Test 3: making a directory with name > 8 characters..."
expect_error "mkdir ${MOUNT}/dir_2long"
echo -n " > "

# Did we fail to make an overlength directory?
if [ -d "${MOUNT}/dir_2long" ]; then fail; else pass; fi

####-------- TEST 4 ---- TEST 4 ---- TEST 4 ---- TEST 4 ---- TEST 4 --------####

# Adding > MAX_DIRS_IN_ROOT directories.
echo
echo " > Test 4: making too many directories..."

# Make 3 extra directories to test the FUSE implementation.
for i in {2..32}; do
  if [ $i -gt 29 ]; then
    expect_error "mkdir ${MOUNT}/dir_${i}"
  else mkdir ${MOUNT}/dir_${i}; fi
done

echo -n " > "

# Do we run out of space on root when we should?
if [ -d "${MOUNT}/dir_29" ]; then
  if [ -d "${MOUNT}/dir_30" ]; then fail; else pass; fi
else fail; fi

####-------- TEST 5 ---- TEST 5 ---- TEST 5 ---- TEST 5 ---- TEST 5 --------####

# Adding a file.
echo
echo -n " > Test 5: adding a file... "
echo "taco" > ${MOUNT}/dir_1/file_1.txt;

# Did we make a file?
if [ -f "${MOUNT}/dir_1/file_1.txt" ]; then pass; else fail; fi

####-------- TEST 6 ---- TEST 6 ---- TEST 6 ---- TEST 6 ---- TEST 6 --------####

# File name > 8 characters.
echo
echo " > Test 6: adding file with name > 8 characters..."
expect_error "> ${MOUNT}/dir_1/fil_2long.txt"
echo -n " > "

# Did we fail to make a file?
if [ -f "${MOUNT}/dir_1/fil_2long.txt" ]; then fail; else pass; fi

####-------- TEST 7 ---- TEST 7 ---- TEST 7 ---- TEST 7 ---- TEST 7 --------####

# File has no extension.
echo
echo " > Test 7: adding file with no extension..."
expect_error "> ${MOUNT}/dir_1/file"
echo -n " > "

# Did we fail to make a file?
if [ -f "${MOUNT}/dir_1/fil_2long.txt" ]; then fail; else pass; fi

####-------- TEST 8 ---- TEST 8 ---- TEST 8 ---- TEST 8 ---- TEST 8 --------####

# File extension > 3 characters.
echo
echo " > Test 8: adding file with extension > 3 characters..."
expect_error " > ${MOUNT}/dir_1/file.text"
echo -n " > "

# Did we fail to make a file?
if [ -f "${MOUNT}/dir_1/fil_2long.txt" ]; then fail; else pass; fi

####-------- TEST 9 ---- TEST 9 ---- TEST 9 ---- TEST 9 ---- TEST 9 --------####

# Adding > MAX_FILES_IN_DIR files.
echo
echo " > Test 9: making too many files..."

# Make 3 extra files to test FUSE implementation.
for i in {2..20}; do
  if [ $i -gt 17 ]; then
    expect_error " > ${MOUNT}/dir_1/file_${i}.txt"
  else
    echo "taco" > ${MOUNT}/dir_1/file_${i}.txt
  fi
done

echo -n " > "

# Did files 18-20 fail?
if [ -f "${MOUNT}/dir_1/file_17.txt" ]; then
  if [ -f "${MOUNT}/dir_1/file_18.txt" ]; then fail; else pass; fi
else fail; fi

####----- TEST 10 ---- TEST 10 ---- TEST 10 ---- TEST 10 ---- TEST 10 ------####

echo
echo "Creating 1mb file..."
dd if=/dev/zero of=1mb.txt count=1024 bs=1024
echo "...done."
echo
echo -n " > Test 10: adding a 1mb file... "

# Adding a 1mb file.
cat 1mb.txt > ${MOUNT}/dir_2/1mb.txt
filesize=$(wc -c <"${MOUNT}/dir_2/1mb.txt")

# Did we create a 1mb file?
if [ $filesize -ge 1000000 ]; then pass; else fail; fi

####----- TEST 11 ---- TEST 11 ---- TEST 11 ---- TEST 11 ---- TEST 11 ------####

clear_disk
mkdir ${MOUNT}/new_dir
echo "taco" > ${MOUNT}/new_dir/first.txt

# Append to an existing file
echo -n " > Test 11: appending to a file... "
echo "cat" >> ${MOUNT}/new_dir/first.txt

# Does our appended file contain 'taco' AND 'cat'?
if grep -q "taco" "${MOUNT}/new_dir/first.txt"; then
  if grep -q "cat" "${MOUNT}/new_dir/first.txt"; then pass; else fail; fi
else fail; fi

####----- TEST 12 ---- TEST 12 ---- TEST 12 ---- TEST 12 ---- TEST 12 ------####

echo "taco" > ${MOUNT}/new_dir/second.txt

echo -n " > Test 12: append to a first.txt,"
echo -n " after creating second.txt after it... "
echo "hey" >> ${MOUNT}/new_dir/first.txt

# Does our appended file contain 'taco' AND 'cat' AND 'hey'?
if grep -q "taco" "${MOUNT}/new_dir/first.txt"; then
  if grep -q "cat" "${MOUNT}/new_dir/first.txt"; then
    if grep -q "hey" "${MOUNT}/new_dir/first.txt"; then pass; else fluke; fi
  else fluke; fi
else fluke; fi

####----- TEST 13 ---- TEST 13 ---- TEST 13 ---- TEST 13 ---- TEST 13 ------####

clear_disk
mkdir ${MOUNT}/test_dir

echo "Creating large (10,000 disk blocks) file..."
echo
dd if=/dev/zero of=5mb.txt count=10000 bs=512
echo "...done."
echo
echo -n " > Test 13: adding a 5mb to a clean disk..."

cat 5mb.txt > ${MOUNT}/test_dir/5mb.txt
filesize=$(wc -c <"${MOUNT}/test_dir/5mb.txt")

if [ $filesize -eq 5120000 ]; then pass; else fail; fi

summary
