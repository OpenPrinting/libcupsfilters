echo "Test-Suite for Libcupsfilters"
#Now run the executable file with test case
echo "Running Tests..."
echo "Input number of Tests..."
#read TC

mkdir -p cupsfilters/test_files/output_files
./testfilters cupsfilters/test-filter-cases.txt 3 2&> Test_summary_final.txt
exit_code=$?

#Delete the output_files folder all together...
rm -r cupsfilters/test_files/output_files
exit $exit_code
