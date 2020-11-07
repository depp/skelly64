"""sizes.py -- print sizes of objects is an executable.

Note that this is broken, we should be parsing program headers.
"""
import subprocess
import sys

def die(*msg):
    print('Error:', *msg, file=sys.stderr)
    raise SystemExit(1)

def main(argv):
    types = {
        'B': 'bss',
        'b': 'bss',
        'C': 'common',
        'D': 'data',
        'd': 'data',
        'G': 'data',
        'g': 'data',
        'n': 'data',
        'R': 'data',
        'r': 'data',
        'S': 'bss',
        's': 'bss',
        'T': 'text',
        't': 'text',
    }
    for arg in argv:
        proc = subprocess.run(
            ['mips32-elf-nm', '-S', arg],
            stdout=subprocess.PIPE)
        if proc.returncode:
            raise SystemExit(1)
        sections = {}
        for section in types.values():
            sections[section] = 0
        total = 0
        for line in proc.stdout.splitlines():
            fields = line.split()
            if len(fields) < 2:
                die('could not parse line:', repr(line))
            stype = fields[-2].decode('ascii')
            ssec = types.get(stype)
            if ssec is not None:
                if len(fields) < 4:
                    die('missing size:', repr(line))
                ssize = int(fields[1], 16)
                # print(ssize, fields[-1])
                sections[ssec] += ssize
                total += ssize
        for sec, ssize in sorted(sections.items()):
            if ssize:
                print('{}: {}'.format(sec, ssize))
        print()
        print('Total:', total)

if __name__ == '__main__':
    main(sys.argv[1:])
