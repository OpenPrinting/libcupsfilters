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
| Attribute              | Description / Example                                         |
|------------------------|---------------------------------------------------------------|
| **copies**             | Number of copies (e.g. `copies=3`)                            |
| **output-order**       | Page order (`reverse`)                                        |
| **number-up**          | Multiple pages per sheet (1,2,3,4,6,8,9,10,12,15,16)          |
| **page-border**        | Border style (`none`, `single`, `double`, etc.)               |
| **page-delivery**      | Stacking order (`same-order-face-down`, …)                    |
| **page-ranges**        | Specific pages (e.g. `page-ranges=1-4,7,9-12`)                |
| **page-set**           | `all`, `odd`, `even`                                         |
| **sides**              | `one-sided`, `two-sided-long-edge`, `two-sided-short-edge`   |
| **media**              | Shorthand size (`iso_a4_210x297mm`, `na_5x7_5x7in`, …)        |
| **media-col**          | Detailed properties (`media-size`, `media-color`, margins…)  |
| **orientation-requested** | 3=Portrait, 4=Landscape, 5=Reverse Landscape, 6=Reverse Portrait |
| **print-color-mode**   | `color` or `monochrome`                                      |
| **print-quality**      | 3=Draft, 4=Normal, 5=High                                    |
| **print-rendering-intent** | Color handling method                                    |
| **print-scaling**      | `auto`, `fit`, `fill`, `none`                                |
| **printer-resolution** | DPI (e.g. `600dpi`, `1200x600dpi`)                           |
| **mirror**             | `true` / `false`                                             |
| **booklet**            | `booklet=true` auto-sets 2-up booklet layout                 |
| **overrides**          | Different attributes for different page ranges               |

Sources of Current Files utilized in Test
-----------------------------------------

| File                               | Source |
|------------------------------------|--------|
| `bashrc.urf`                       | CUPS   |
| `filled-2.pdf`                     | CUPS   |
| `form_english.pdf`                 | CUPS   |
| `onepage-a4-adobe-rgb-8-150dpi.pwg`| PWG sample |
| `test_file.pdf`                    | CUPS   |
| `test_file.pwg`                    | CUPS   |
| `test_file_1pg.pdf`                | CUPS   |
| `test_file_2pg.pdf`                | CUPS   |
| `test_file_4pg.pdf`                | CUPS   |


If more files are required by you for testing your additions or enhance the testing suite, you can visit the following sites:
- [Printer Working Group](https://www.pwg.org/)
- [Sample Files by OpenPrinting](https://github.com/OpenPrinting/sample-files)
- [Sample Files by CUPS](https://github.com/OpenPrinting/cups/tree/master/examples)
- [Sample Files by PAPPL](https://github.com/michaelrsweet/pappl/tree/master/testsuite)
- [Sample PCLm Files By Sahil Arora](https://github.com/sahilarora535/raster-to-pclm)
