bladeRF Version Change Checklist
================================================================================

This document describes tasks that must be performed when updating the
version numbers of various components contained in this repository.

Build and functional tasks are a given are documented [elsewhere][tests] and
are not covered here.

[tests]: test_checklist.md

When multiple components in the repo have changed, this checklist should be
performed in the following order:

* FPGA
* FX3 Firmware
* libbladeRF
* bladeRF-cli
* Project-wide items

--------------------------------------------------------------------------------


FPGA
================================================================================

* [ ] Update the ```hdl/CHANGELOG``` file

* [ ] Update the version number in the NIOS II ```fpga_version.h``` files (one
      per platform)

* [ ] Update the version compatibilty table in the libbladeRF
      ```compatibility.c``` files (one per platform)

* [ ] Apply the tag: ```fpga_vX.Y.Z```

* [ ] Generate the official bitstreams ***at*** the tag and have them uploaded
      to https://www.nuand.com/fpga

* [ ] Update the ```bladerf-fpga-hostedx*.postinst``` files under
      ```host/debian/```.



FX3 Firmware
================================================================================

* [ ] Update the ```fx3_firmware/CHANGELOG``` file

* [ ] Update the version number in the FX3 firmware CMakeLists.txt file

* [ ] Update the version compatibility table in the libbladeRF
      ```version_compat.c``` file.

* [ ] Apply the tag: ```firmware_vX.Y.Z```

* [ ] Generate the official image ***at*** the tag with the following CMake config:
    * ```-DTAGGED_RELEASE=Yes``` (If building from top-level)
    * ```-DVERSION_INFO_EXTRA=""``` (If building in fx3_firmware)

* [ ] Have the image uploaded to https://www.nuand.com/fx3

* [ ] Update the ```bladerf-firmware-fx3.postinst``` file under ```host/debian```.


libbladeRF
================================================================================

* [ ] Update the ```libbladeRF/CHANGELOG``` file

* [ ] Update the version number in ```libbladeRF/CMakeLists.txt```

* [ ] Update the version number in ```libbladeRF/include/libbladeRF.h```

* [ ] Apply the tag: ```libbladeRF_vX.Y.Z```

* [ ] Generate Doxygen pages ***at*** the tag with:
    * ```-DBUILD_DOCUMENTATION=ON```
    * ```-DTAGGED_RELEASE=Yes``` (If building from top-level)
    * ```-DVERSION_INFO_EXTRA=""``` (If building in ```host/```)

* [ ] Gather the Doxygen pages from
      ```host/build/libraries/libbladeRF/doc/doxygen/html/``` and have them
      uploaded to https://www.nuand.com/bladeRF-doc/libbladeRF/vX.Y.Z/

bladeRF-cli
================================================================================

* [ ] Regenerate the fallback ```cmd_help.*``` items using the ```generate.bash```
      script. (This is in ```src/cmd/doc/```.)

* [ ] Update the version number in the CMakeLists.txt file

* [ ] Update the ```bladeRF-cli/CHANGELOG``` file

* [ ] Apply the tag ```bladeRF-cli_vX.Y.X```

MATLAB Bindings
================================================================================

* [ ] Regenerate ```libbladeRF_proto.m``` if required (TODO: how?)

* [ ] Update the ```CHANGELOG``` file

* [ ] Apply the tag: ```matlab_vX.Y.X```

Python Bindings
================================================================================

* [ ] Regenerate ```_cdef.py``` if required (TODO: how?)

* [ ] Update the version number in the ```setup.py``` file

* [ ] Update the ```CHANGELOG``` file

* [ ] Apply the tag: ```python_vX.Y.X```

Project-wide items
================================================================================

* [ ] Ensure ***all*** of the above items are at a tagged version

* [ ] Update the top-level CHANGELOG file.

* [ ] Update the ```debian/changelog``` file using ```dch -i```

* [ ] Apply the tag: ```YYYY.MM[-rcN]```

* [ ] Prepare the API docs and binary packages
