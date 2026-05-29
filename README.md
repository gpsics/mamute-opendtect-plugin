# Mamute Seismic Tools — OpendTect Plugin

**A plugin that brings Mamute's tools directly into the OpendTect interpretation environment.**

---

## Description

[Mamute](https://lappsufrn.gitlab.io/mamute/dev/) is an open-source, high-performance computing software developed at the
[Laboratory of Parallel Architectures for Signal Processing (LAPPS)](https://lapps.imd.ufrn.br/) at the Universidade Federal
do Rio Grande do Norte (UFRN). It implements wave equation-based geophysical methods, including viscoacoustic seismic modeling 
and full waveform inversion (FWI) using high-performance strategies such as MPI-based distributed computing and OpenMP parallelism.

[OpendTect](https://dgbes.com/software/opendtect) is a free, open-source seismic interpretation platform developed by dGB Earth Sciences, 
used worldwide for visualizing, analyzing and interpreting seismic data.

This plugin integrates Mamute into OpendTect as a native GUI plugin, allowing users to configure, execute and inspect seismic 
simulations without leaving the OpendTect environment.

---

## Features

- Configure and run visco-acoustic modeling directly from OpendTect
- Define acquisition geometry (sources and receivers) interactively
- Generate velocity and attenuation (Qp) models using built-in generators
- Real-time execution monitoring with log display
- Inspect and import seismogram outputs into OpendTect's visualization tools

---

## Configuration

Once the plugin is loaded in OpendTect, set the Mamute installation path via:

**Menu Bar -> Mamute Seismic Tools -> Settings**

---

## License

This project is licensed under the MIT License. See the `MIT` file for details.
