# uwapppos example

Transmit messages received by TCP between nodes and dynamically change the node positions

The example generates the ns TCL configuration from a template and consists of:
- one receiving node
- a configurable number of sending nodes

The sending nodes get the messages to transmit from the Python script `test_sockets.py` via a TCP connection (each node opens a unique TCP port) and the position of the receiving node is changed by the script using an UDP socket connection to ns2. 

## Requirements

- Python 3.6 or newer
- psutil and (optional colorlog) packages, install with `pip3 install psutil [colorlog]`

## Usage

Run `python3 test_sockets.py` to get the help information:

```
python3 test_sockets.py --help
usage: test_sockets.py [-h] [-n NUM_NODES] [-t RUN_TIME] [-v] -b BUILD_DIR

Run network example with node position updates, see uwAppPos_UDP.tmpl for configuration.

options:
  -h, --help            show this help message and exit
  -n NUM_NODES, --num-nodes NUM_NODES
                        Number of sending nodes
  -t RUN_TIME, --run-time RUN_TIME
                        Run simulation for given number of seconds
  -v, --verbose         Increase Logger output level, up to three times
  -b BUILD_DIR, --build-dir BUILD_DIR
                        DESERT build directory
```


You need to provide at least the **DESERT build directory** to locate `ns` and the addon modules, e.g.:

`python3 test_sockets.py -b  ~/DESERT_Underwater/DESERT_buildCopy_LOCAL`

