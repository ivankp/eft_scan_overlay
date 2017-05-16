### Compilation

Run `make`.

`eft_scan_hists` executable will be generated in this directory.

### Usage

E.g. `./eft_scan_hists yoda_plots plots.pdf`

The `eft_scan_hists` executable takes two arguments. The first one is a path
(absolute or relative) to the directory containing subdirectories corresponding
to points in the Wilson coefficients parameters space. The subdirectories are
expected to contain two files: `param.dat` and `Higgs.yoda`. The second
argument is the output root or pdf file name to which the histograms are going
to be written.
