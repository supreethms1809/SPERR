[![clang-format](https://github.com/NCAR/SPERR/actions/workflows/clang-format.yml/badge.svg)](https://github.com/NCAR/SPERR/actions/workflows/clang-format.yml)
[![clang-tidy](https://github.com/NCAR/SPERR/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/NCAR/SPERR/actions/workflows/clang-tidy.yml)
[![unit-test](https://github.com/NCAR/SPERR/actions/workflows/unit-test.yml/badge.svg)](https://github.com/NCAR/SPERR/actions/workflows/unit-test.yml)
[![CodeQL](https://github.com/NCAR/SPERR/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/NCAR/SPERR/actions/workflows/codeql-analysis.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/5fb9befd9687440195ca739ec60abc39)](https://www.codacy.com/gh/shaomeng/SPERR/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=shaomeng/SPERR&amp;utm_campaign=Badge_Grade)


[![DOI](https://zenodo.org/badge/225491235.svg)](https://zenodo.org/badge/latestdoi/225491235)


## Overview

SPERR (pronounced like *spur*) is a lossy compressor for scientific data (2D or 3D floating-point data, mostly produced by numerical simulations). 
SPERR has one of the highest coding efficiencies among popular lossy compressors, meaning that it usually uses the least amount of storage
to satisfy a prescribed error tolerance (e.g., a maximum point-wise error tolerance).


Under the hood, SPERR uses wavelet transforms, [SPECK](https://ieeexplore.ieee.org/document/1347192) coding, 
and a custom outlier coding algorithm in its compression pipeline. 
This combination gives SPERR flexibility to compress targetting different quality controls, namely 1) bit-per-pixel (BPP), 
2) peak signal-to-noise ratio (PSNR), and 3) point-wise error (PWE).
The name of SPERR stands for **SP**eck with **ERR**or bounding.

## Documentation

SPERR documentation is hosted on Github [Wiki](https://github.com/NCAR/SPERR/wiki) pages. To get started, one might want to
[build SPERR from source](https://github.com/NCAR/SPERR/wiki/Build-SPERR-From-Source) and explore compression and decompression
utilities for [3D](https://github.com/NCAR/SPERR/wiki/CLI:-3D-Compression-and-Decompression-Utilities) and [2D](https://github.com/NCAR/SPERR/wiki/CLI:-2D-Compression-and-Decompression-Utilities) inputs.
One may also want to pull a [docker image](https://hub.docker.com/r/shaomeng/sperr-docker)
which contains SPERR in a complete development environment. 
Finally, a collection of canonical scientific data sets is available at [SDRBench](https://sdrbench.github.io/) for testing and evaluation purposes.

SPERR also provides programming [API in C++ and C](https://github.com/NCAR/SPERR/wiki#sperr-c-api).

