Testing for libcupsfilters
=============================

Please see the [Developing for libcupsfilters](DEVELOPING.md) file for
information on developing for the libcupsfilters project.

Please see the [Contributing to libcupsfilters](CONTRIBUTING.md) file for
information on contributing to the libcupsfilters project.

How To Contact The Developers
-----------------------------

The Linux Foundation's "printing-architecture" mailing list is the primary means
of asking questions and informally discussing issues and feature requests with
the OpenPrinting developers.  To subscribe or see the mailing list archives, go
to <https://lists.linuxfoundation.org/mailman/listinfo/printing-architecture>.

Feel free to also post questions in the "issues" section of GitHub Page.

How To Set the Testing Environment
----------------------------------

There are 2 ways to test changes being made by you in the libcupsfilters library:

### 1. CI/CD based testing
- All pull requests and pushes are automatically built and tested by the CI system.  
- To reproduce these tests locally:

```bash
make check
```

### 2. Manual testing
- You require compiled source code of [cups-filters](https://github.com/OpenPrinting/cups-filters).
- You can test the changes manually by calling the following:
```bash
cat PATH/TO/INPUT/FILE.filetype | CONTENT_TYPE=<"INPUT_TYPE"> FINAL_CONTENT_TYPE=<"OUTPUT_TYPE"> LD_LIBRARY_PATH=.libs PATH/TO/cups-filters/pdftopdf 1 1 1 1 'PRINTING ATTRIBUTIONS' > PATH/TO/OUTPUT/FILE.filetype
```
- Printing attributions are given in next sections.


Adding New Test Cases in CI/CD Environment
------------------------------------------

- You can extend the test suite by adding :

### 1. Input test files under:
```bash
cupsfilters/test_files/
```

### 2. Case definitions in:
```bash
cupsfilters/test-filter-cases.txt
```

- Each line in test-filter-cases.txt follows this format:
```bash
Input_File Input_Type Output_File Output_Type Make Model Color Duplex Formats Job-Id: random number User: randome name Title: randome title Copies: range between 1 to 20 Options
```
If you have a doubt in understanding how this format works, have a look at how cases are defined in "cupsfilters/test-filter-cases.txt". PRO-TIP: the values seperated by tab goes to next attribute.

Printing Attributes
-------------------

The current implementation of pdfio supports many printing attributes as follows:

### 1. **copies**: A number that specifies how many copies of the entire job to print  
copies=3 will print three copies of the document(s).

### 2. **output-order**: Specifies the order in which the pages should be printed.
reverse will print the pages in reverse order, from the last page to the first.

### 3. **number-up**: This powerful feature allows you to print multiple pages of a document on a single sheet of paper
number-up=2 will print two pages side-by-side on each sheet. The supported values are 1, 2, 3, 4, 6, 8, 9, 10, 12, 15, 16.

### 4. **page-border**: You can add a border around each page on the sheet. This is especially useful with number-up.
The options are none, single, single-thick, double, and double-thick.

### 5. **page-delivery**: This attribute controls the stacking order and orientation of the output pages
The options are same-order-face-down, same-order-face-up, reverse-order-face-down, and reverse-order-face-up.

### 6. **page-ranges**: If you only want to print specific pages, you can specify them with this attribute
For example, page-ranges=1-4,7,9-12 will print pages 1 through 4, page 7, and pages 9 through 12.

### 7. **page-set**: A simpler way to select pages. 
The options are all, odd, and even.

### 8. **sides**: This attribute controls whether printing is done on one or both sides of the paper. 
Common values are one-sided, two-sided-long-edge (for duplex printing, flipping on the long edge), and two-sided-short-edge (for duplex printing, flipping on the short edge).

### 9. **media**: A shorthand way to specify the media size. 
The accepted media values are as follows:
    - iso_a4_210x297mm (This is the default value if no media is specified)
    - iso_a6_105x148mm
    - na_index-4x6_4x6in
    - na_5x7_5x7in
    - na_govt-letter_8x10in
    - Any media name containing the string "photo" (e.g., na_4x6-photo_4x6in)

### 10. **media-col**: A more detailed way to specify media properties. 
This is a collection of attributes that can include:
    - media-size-name: The standard name of the media size.
    - media-size: The dimensions of the media, specified with x-dimension and y-dimension.
    - media-color: The color of the paper (e.g., white, blue). 
    - media-source: The paper tray to use (e.g., tray-1, manual). 
    - media-type: The type of paper (e.g., stationery, photographic).
    - media-bottom-margin, media-left-margin, media-right-margin, media-top-margin: The unprintable margins of the media.

### 11. **orientation-requested**: This specifies the orientation of the content on the page. 
The possible values are:
    - 3: Portrait
    - 4: Landscape
    - 5: Reverse Landscape
    - 6: Reverse Portrait

### 12. **print-color-mode**: Sets the color mode for the print job, 
such as color or monochrome.

### 13. **print-quality**: This allows you to control the print quality, which can affect both the appearance and the printing speed. 
The options are:
    - 3: Draft quality
    - 4: Normal quality
    - 5: High quality

### 14. **print-rendering-intent**: Specifies how colors should be handled, which is important for color-critical work.

### 15. **print-scaling**: This attribute determines how the document content is scaled to fit on the page. 
The options are auto, auto-fit, fill, fit, and none.

### 16. **printer-resolution**: You can specify the desired print resolution in dots per inch (DPI), 
for example, 600dpi or 1200x600dpi.

### 17. **mirror**: A boolean value (true or false) that, when set to true, will print a mirrored image of the document

### 18. **booklet**: A special mode that arranges the pages in a way that they can be folded and stapled to create a booklet. 
When booklet=true, it automatically sets number-up=2 and arranges the pages in the correct order for booklet making.

### 19. **overrides**: This is an advanced feature that allows you to specify different printing attributes for different parts of the job. 
For example, you can use overrides to print certain pages on different paper or with a different orientation.

Sources of Current Files utilized in Test
-----------------------------------------

1) bashrc.urf - CUPS 
2) filled-2.pdf - CUPS 
3) form_english.pdf - CUP
4) onepage-a4-adobe-rgb-8-150dpi.pwg - PWG file by [Printer Working Group](https://ftp.pwg.org/pub/pwg/ipp/examples/)
5) test_file.pdf - CUPS 
6) test_file.pwg - CUPS
7) test_file_1pg.pdf - CUPS
8) test_file_2pg.pdf - CUPS
9) test_file_4pg.pdf - CUPS

If more files are required by you for testing your additions or enhance the testing suite, you can visit the following sites:
- [Printer Working Group](https://www.pwg.org/)
- [Sample Files by OpenPrinting](https://github.com/OpenPrinting/sample-files)
- [Sample Files by CUPS](https://github.com/OpenPrinting/cups/tree/master/examples)
- [Sample Files by PAPPL](https://github.com/michaelrsweet/pappl/tree/master/testsuite)
- [Sample PCLm Files By Sahil Arora](https://github.com/sahilarora535/raster-to-pclm)
