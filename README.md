### Compilation

Run `make`.

`plot` executable will be generated in this directory.

### Usage

`./plot yoda_plots plots.pdf`

`./plot yoda_plots histograms.root`

The `plot` executable takes two arguments. The first one is a path (absolute or
relative) to the directory containing subdirectories corresponding to points in
the Wilson coefficients parameters space. The subdirectories are expected to
contain two files: `param.dat` and `Higgs-scaled.yoda`. The second argument is
the output root or pdf file name to which the histograms are going to be
written.

### Dependensies

`boost::filesystem`, `ROOT`
