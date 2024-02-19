import psutil

def get_process_id_by_name(process_name) -> list:
    """
    Get process(es) in python by name

    processes_python = get_process_id_by_name('python')
    for p in processes_python:
        print("PID", p.pid)
    """
    return [proc for proc in psutil.process_iter() if proc.name() == process_name]


def get_process_id_by_name_port(process_name, port):
    """
    Get a process in python by name and port number

    process_python_8080 = get_process_by_name_port('python.exe', port)
    print("PID", process_python_8080.pid)
    """
    processes = [proc for proc in psutil.process_iter() if proc.name()
                 == process_name]
    for p in processes:
        for c in p.connections():
            if c.status == 'LISTEN' and c.laddr.port == port:
                return p
    return None
