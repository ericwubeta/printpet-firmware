"""QEMU GDB remote helper; never connects to a physical device."""
import socket, struct

class Remote:
    def __init__(self, port):
        self.s=socket.create_connection(('127.0.0.1',port),timeout=5)
        self.s.settimeout(2)
    def receive(self, timeout=2):
        self.s.settimeout(timeout)
        while True:
            b=self.s.recv(1)
            if b==b'$': break
            if not b: raise EOFError()
        out=b''
        while True:
            b=self.s.recv(1)
            if b==b'#':break
            out+=b
        checksum=self.s.recv(2)
        self.s.sendall(b'+')
        return out.decode('ascii',errors='replace')
    def send(self,command,timeout=2):
        b=command.encode();self.s.sendall(b'$'+b+b'#'+f'{sum(b)%256:02x}'.encode())
        return self.receive(timeout)
    def run(self, timeout):
        try:return self.send('c',timeout)
        except socket.timeout:
            self.s.sendall(b'\x03');return self.receive(5)
    def regs(self):
        data=bytes.fromhex(self.send('g')); regs=struct.unpack('<'+'I'*(len(data)//4),data)
        return dict(pc=hex(regs[32]),sp=hex(regs[2]),ra=hex(regs[1]),registerBytes=len(data))
