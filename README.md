# TLM2 interfaces

TLM2 based interface definitions for AXI, ACE and CHI compliance.
The files allow development against a standard interface definition
which will be compliant with other vendor provided IP models for
instance model provided by Arteris.

For installation follow the following steps:

mkdir build
cmake -DCMAKE_PREFIX_PATH=$SYSTEMC_HOME ..
make install

The library depends on SystemC and is tested against SystemC version
2.3.3 with gcc-6.3. Other combinations will most likely also work.


