# uwapp_net DESERT app layer addon

This addon bases on uwApplication DESERT module with the following differences:

- currently only supports TCP connections to receive / send messages
- TCP messages need a structure with a begin tag (`int32_t`) and the message length 
  (to support binary messages), all sent with **little endian** encoding
  
    | Byte index | Type  | Contents            |
    |------------|-------|---------------------|
    | 0          | int32 | Message begin tag   |
    | 4          | int32 | Message data length |
    | 8          | char  | Message data        |
    
    
The addon was tested with DESERT version 3.4.1. 

## Installation

The addon may be installed in two ways:

- copy the directory to DESERT_Addons and install DESERT as usual (but activate the addon during installation)
- use the build script `build.sh` that compiles and installs the addon to an existing DESERT build environment:

  Call the script with the root (`-r`) and build directories (`-b`) as arguments:

    ```sh
    ./build.sh -r <DESERT_ROOT> -d <DESERT_BUILD>
    ```
  e.g.:

    ```sh
    ./build.sh -r ~/DESERT_Underwater -b ~/DESERT_Underwater/DESERT_buildCopy_LOCAL
    ```
    
## Example

There is a Python sample application in the `sample` directory. 
