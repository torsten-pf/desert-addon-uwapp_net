#!/usr/bin/env python3

import sys
import os
from datetime import datetime
import errno
import logging  
import re
import select
import signal
import socket
from string import Template
import struct
import subprocess
import threading
import time
from argparse import ArgumentParser

from process_utils import get_process_id_by_name

try:
    import colorlog
    handler = colorlog.StreamHandler()
    handler.setFormatter(colorlog.ColoredFormatter('%(log_color)sPYTHON %(asctime)s %(levelname)s: %(message)s', datefmt='%H:%M:%S', force_color =True))

    logger = colorlog.getLogger(__file__)
    logger.addHandler(handler)
except Exception as e:
    print(e)
    logger = logging.getLogger(__file__)
    logging.basicConfig(format='PYTHON %(asctime)s.%(msecs)03d %(levelname)s: %(message)s', level=logging.DEBUG, datefmt='%H:%M:%S')

# 7-bit and 8-bit C1 ANSI sequences
ansi_escape_8bit = re.compile(br'''
    (?: # either 7-bit C1, two bytes, ESC Fe (omitting CSI)
        \x1B
        [@-Z\\-_]
    |   # or a single 8-bit byte Fe (omitting CSI)
        [\x80-\x9A\x9C-\x9F]
    |   # or CSI + control codes
        (?: # 7-bit CSI, ESC [ 
            \x1B\[
        |   # 8-bit CSI, 9B
            \x9B
        )
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
''', re.VERBOSE)

MAGIC = 0x20041969

class CustomTemplate(Template):
	delimiter = '%$%'


HOST = "127.0.0.1"  # The server's hostname or IP address
UW_APP_PORT_BASE = 4000
NUM_SEND_NODES = 2
UW_APP_UDP_POS_PORT_BASE = 5000

class RepeatTimer(threading.Timer):  
    def run(self):
        while not self.finished.wait(self.interval):  
            self.function(*self.args,**self.kwargs)  


class SingleNode(threading.Thread):
    def __init__(self, node_id: int, send_interval: float, send_to_host: str, send_to_port: int):
        """
        id: node id
        send_interval: send interval in [s], set to 0.0 to not send messages
        """
        super().__init__(name=f"TcpNodeThread{node_id}")
        self.node_id = node_id
        self.send_interval = send_interval
        self.should_stop = False
        self.last_send_dt = None
        self.connect_address = (send_to_host, send_to_port)
        self.sent_pckt_cnt = 0

    def stop(self):
        """stop the thread."""
        self.should_stop = True

    def _recv_wait(self, s: socket.socket, siz: int) -> bytes:
        """Receive siz data bytes for non-blocking socket"""
        d = bytes()
        while len(d) < siz:
            try:
                d += s.recv(siz-len(d))
            except socket.error as e:
                if e.errno != errno.EAGAIN:
                    raise e
        return d

    def _recv_data(self, sock_list) -> tuple[bool, bytes]:
        """Receive AcoSim message with header"""
        for s in sock_list:
            # read magic
            size_data = s.recv(4)
            if size_data:
                magic = struct.unpack("<I", size_data)[0]
                if magic != MAGIC:
                    logger.warning(f"Wrong magic 0x{magic:08x} found, skipping")
                    return True, b""
            # read size
            size_data = s.recv(4)
            if size_data:
                size = struct.unpack("<I", size_data)[0]
                if size > 0:
                    # data = s.recv(header[1])                         
                    data = self._recv_wait(s, size)
                    if data:
                        # A readable client socket has data
                        return True, data
                    else:
                        # Interpret empty result as closed connection
                        return False, b""
            else:
                # Interpret empty result as closed connection
                return False, b""
        return True, b""

    def _send_data(self, s: socket.socket, id:int, msg: bytes) -> int:
        total_sent = 0
        # append counter to message
        msg = msg + bytes(f" #{self.sent_pckt_cnt+1}", encoding="utf-8")
        data = struct.pack("<II", MAGIC, len(msg)) + msg
        # data = msg    
        while len(data):
            try:
                sent = s.send(data)
                total_sent += sent
                data = data[sent:]
            except socket.error as e:
                if e.errno != errno.EAGAIN:
                    raise e
                return -total_sent
            except BrokenPipeError as e:
                return -total_sent
        self.last_send_dt = datetime.now()
        self.sent_pckt_cnt += 1
        logger.info(f"Node {self.node_id} sent message {self.sent_pckt_cnt}: '{msg.decode()}'")
        return total_sent

    def connect_socket(self, s: socket.socket, address: tuple, retries: int = 5) -> bool:
        err = 'unknown'
        counter = 0
        while counter < retries:
            try:
                s.connect(address)
                return True  
            except socket.error as error:
                err = error
                # print(f"Connection to {address} failed, reason: {error}")
                # print(f"Attempt {counter} of {retries}")
                counter += 1
            time.sleep(1.0)
        logger.error(f"Connection to {address} failed {retries} times, reason: {err}")
        return False

    def run(self):
        """ Worker thread function """
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:                
            logger.info(f"Node {self.node_id}: connecting to {self.connect_address}")
            #s.connect((HOST, UW_APP_PORT_BASE+id))
            if not self.connect_socket(s, self.connect_address):
                return
            logger.debug(f"Node {self.node_id} connected to {self.connect_address}")
            # s.setblocking(0)
            s.settimeout(0.1)
            msg = bytes(f"Message from node {self.node_id}", encoding="utf-8")
            send_timer = RepeatTimer(interval=self.send_interval, function=self._send_data, args=(s,id,msg))
            if self.send_interval > 0.0:
                send_timer.start()
            while not self.should_stop:
                readable, _, _ = select.select([s], [], [], 0.1)
                success, data = self._recv_data(readable)
                if not success:
                    logger.warning(f"Node {self.node_id}: disconnected from {self.connect_address}")
                    break
                if data:
                    n = datetime.now()
                    logger.info(f'Node {self.node_id} received message: {data.decode()}')
            send_timer.cancel()  


class PosWorker(threading.Thread):
    def __init__(self, node_id: int,
                 send_interval: float,
                 send_address: tuple,
                 pos_data: dict,
                 speed: dict = None) -> None:
        """
        id: node id
        send_interval: send interval in [s], set to 0.0 to not send messages
        """
        super().__init__(name=f"PosWorker{node_id}")
        if speed is None:
            speed = {"x": 1.0, "y": 0.0, "z": 1.5}
        self.node_id = node_id
        self.send_interval = send_interval
        self.should_stop = False
        self.send_address = send_address
        self.pos_data = pos_data
        self.speed = speed

    def stop(self):
        """stop the thread."""
        self.should_stop = True

    def run(self):
        """Worker thread sending the position data"""
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            logger.info(f"Sending position data for node  {self.node_id} to {self.send_address}")
            # s.setblocking(False)
            last_pos_update = 0.0
            while not self.should_stop:
                try:
                    now_dt = datetime.utcnow()
                    now = time.mktime(now_dt.timetuple()) + now_dt.microsecond / 1e6
                    if last_pos_update > 0.0:
                        dt = now - last_pos_update
                        if dt > 0.0:
                            self.pos_data["x"] += dt * self.speed["x"]
                            self.pos_data["y"] += dt * self.speed["y"]
                            self.pos_data["z"] += dt * self.speed["z"]
                    last_pos_update = now
                    logger.info(f"Sending position for node {self.node_id}: {self.pos_data}")
                    packed_data = struct.pack("<ddd", self.pos_data["x"], self.pos_data["y"], self.pos_data["z"])
                    s.sendto(packed_data, self.send_address)
                except Exception as e:
                    logger.error(f"Error sending position data for node {self.node_id}: {e}", exc_info=True)
                time.sleep(5.0)
            logger.info(f"Sending position data for node  {self.node_id} stopped")


def output_reader(proc, filename_out, filename_err):
    remove_ansi_escape_sequences_in_file = True
    file_out = open(filename_out, 'w')
    file_err = open(filename_err, 'w')
    os.set_blocking(proc.stdout.fileno(), False)
    os.set_blocking(proc.stderr.fileno(), False)
    while (proc.returncode == None):
        line = proc.stdout.readline()
        if line:
            sys.stdout.buffer.write(line)
            sys.stdout.flush()
            if remove_ansi_escape_sequences_in_file:
                file_out.buffer.write(ansi_escape_8bit.sub(b'', line))
            else:
                file_out.buffer.write(line)
            continue
        #else:
        #    break
        line = proc.stderr.readline()
        if line:
            logger.error("ns: " + line.decode())
            file_err.buffer.write(line)
            continue
        # sleep only if no data was read from both
        time.sleep(0.2)
    logger.info("output_reader(): observed process terminated, closing files")
    file_out.close()
    file_err.close()


# In your destination folder chosen during installation process:
# source environment (if you chose "development" installation mode)
# ./make_environment.sh && source environment (if you chose "release" installation mode)
start_script_template = """#!/bin/bash

set -eo pipefail

BUILD_DIR=%$%{BUILD_DIR}
START_SCRIPT=%$%{START_SCRIPT}

[ -f $BUILD_DIR/environment ] || ([ -f $BUILD_DIR/make_environment.sh ] && pushd $BUILD_DIR && ./make_environment.sh && popd)
. $BUILD_DIR/environment

ns $START_SCRIPT
"""


def main():    
    argparser = ArgumentParser(description='Run network example with node position updates, see uwAppPos_TCP.tmpl for configuration.')
    argparser.add_argument('-n', '--num-nodes', type=int, default=NUM_SEND_NODES, help='Number of sending nodes')
    argparser.add_argument('-s', '--no-ns-start', action='store_true', help='Do not start ns')  # for remote execution
    argparser.add_argument('-t', '--run-time', type=int, default=20, help='Run simulation for given number of seconds')
    argparser.add_argument('-v', '--verbose', action='count', default=0, help="Increase Logger output level, up to three times")
    argparser.add_argument('-b', '--build-dir', required=True, help='DESERT build directory')

    args = argparser.parse_args()
    start_ns = not args.no_ns_start

    logger.setLevel((logging.ERROR, logging.WARN, logging.INFO, logging.DEBUG)[min(args.verbose, 3)])

    script = 'uwAppPos_TCP'
    # create ns2 tcl script
    with open(f'{script}.tmpl', 'rt') as f:
        s = CustomTemplate(f.read())
        out = s.substitute(TMPL_NO_SENDERS=args.num_nodes,
                           TMPL_PROTOCOL="tcp",
                           TMPL_STOPTIME=args.run_time, 
                           TMPL_APP_PORT_BASE=UW_APP_PORT_BASE, 
                           TMPL_APP_POS_PORT_BASE=UW_APP_UDP_POS_PORT_BASE)
        with open(f'{script}.tcl', 'wt') as f1:
            f1.write(out)
    
    # create n2 start script
    s = CustomTemplate(start_script_template)
    out = s.substitute(BUILD_DIR=args.build_dir, START_SCRIPT=f'{script}.tcl')
    with open('run.sh', 'wt') as f:
        f.write(out)
    if start_ns:
        # start process
        ns_proc = subprocess.Popen(['/bin/bash', './run.sh'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        t1 = threading.Thread(target=output_reader, args=(ns_proc, 'ns_run.log', 'ns_run.err'))
        t1.start()
        
        ns_start_time = time.time()  # record start time to observe runtime

    threads = []    
    threads.append(SingleNode(1, 0.0, HOST, UW_APP_PORT_BASE + 1))
    for i in range(args.num_nodes):
        print(f"Creating send node {i + 2}")
        threads.append(SingleNode(i + 2, 5.0, HOST, UW_APP_PORT_BASE + i + 2))
    threads.append(PosWorker(1,
                             5.0,
                             (HOST, UW_APP_UDP_POS_PORT_BASE + 1),
                             {
                                 "x": 0.0,
                                 "y": 0.0,
                                 "z": 100.0,
                             }
                             ))
    for t in threads:
        t.start()
    try:
        while len(threads) > 0:
            time.sleep(1.0)
            if start_ns:
                # check if double of run time is elapsed and stop ns2 by SIGTERM
                if (time.time() - ns_start_time) > 2*args.run_time:
                    logger.info("Double of run time is over, stopping ns2")
                    ns_proc_objects = get_process_id_by_name("ns")
                    for p in ns_proc_objects:
                        os.kill(p.pid, signal.SIGTERM)
                    ns_start_time = time.time()  # reset start time to not kill the processes again in next loop
            
                # stop position worker threads
                if len(get_process_id_by_name("ns")) == 0:
                    for thread in threads:
                        thread.stop()
            
            for thread in threads:
                if not thread.is_alive():
                    thread.join()
                    threads.remove(thread)            
        logger.info('All thread connections closed.')
        if start_ns:
            ns_proc.terminate()  # terminate bash in which ns was running - this stops the output_reader() thread 
            t1.join()
    except KeyboardInterrupt:
        pass    


if __name__ == '__main__':
    main()
