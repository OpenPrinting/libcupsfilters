#! /bin/bash
# License: Apache 2.0
set -x
for f in *.ps
do ps2pdf12 $f ${f%.ps}.pdf
done

