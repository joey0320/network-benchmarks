import subprocess
import os

CC=["riscv64-unknown-elf-gcc"]
CFLAGS=["-mcmodel=medany", "-Wall", "-O2", "-fno-common", "-fno-builtin-printf"]
LDFLAGS=["-T", "link.ld", "-static", "-nostdlib", "-nostartfiles", "-lgcc"]

def compile(source_name, target_name, macros):
    macro_flags = ["-D{}={}".format(name, macros[name]) for name in macros]
    command = CC + CFLAGS + macro_flags + ["-c", source_name, "-o", target_name]
    print(' '.join(command))
    return subprocess.call(command)

def link(source_names, target_name):
    command = CC + LDFLAGS + source_names + ["-o", target_name]
    print(' '.join(command))
    return subprocess.call(command)

def mac_to_hex(macaddr):
    return '0x' + ''.join(reversed(macaddr.split(":")))

SERVER_MACS = ["00:12:6D:00:01:{:02d}".format(i) for i in range(3, 10, 2)]
CLIENT_MACS = ["00:12:6D:00:01:{:02d}".format(i) for i in range(2, 10, 2)]

def main():
    if not os.path.isdir("testbuild"):
        os.makedirs("testbuild")

    compile("crt.S", "testbuild/crt.o", {})
    compile("syscalls.c", "testbuild/syscalls.o", {})

    for (i, (server_mac, client_mac)) in enumerate(zip(SERVER_MACS, CLIENT_MACS)):
        SERVER_BASE = "testbuild/bw-test-server-{}".format(2 * i + 1)
        CLIENT_BASE = "testbuild/bw-test-client-{}".format(2 * i)
        compile(
            "bw-test-server.c",
            SERVER_BASE + ".o",
            {"CLIENT_MACADDR": mac_to_hex(client_mac)})
        compile(
            "bw-test-client.c",
            CLIENT_BASE + ".o",
            {"SERVER_MACADDR": mac_to_hex(server_mac)})
        link([SERVER_BASE + ".o", "testbuild/crt.o", "testbuild/syscalls.o"], SERVER_BASE + ".riscv")
        link([CLIENT_BASE + ".o", "testbuild/crt.o", "testbuild/syscalls.o"], CLIENT_BASE + ".riscv")

if __name__ == "__main__":
    main()
