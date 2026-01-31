#!/bin/sh

echo "Test-Suite for Libcupsfilters"
#Now run the executable file with test case
echo "Running Tests..."
echo "Input number of Tests..."
#read TC

mkdir -p cupsfilters/test_files/output_files
TESTFILE="cupsfilters/test-filter-cases.txt"

# Count all the lines in testcase file. This is will automatically count the number of testcases in turn removing the need for
# hard coding the number of test cases each and everytime someone makes change to test-filter-cases.txt file
NUM_LINES=$(wc -l < "$TESTFILE")

./testfilters "$TESTFILE" "$NUM_LINES" 2 2>&1 | tee Test_summary_final.txt
exit_code=$?

#Delete the output_files folder all together...
rm -r cupsfilters/test_files/output_files
exit $exit_code
