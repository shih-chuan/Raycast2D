import sys, timeit

class Writer:
    def __init__(self, streams):
        self.streams = streams
    def write(self, msg):
        for stream in self.streams:
            stream.write(msg)

def benchmark():

    setup = '''
from _raycast2d import litArea, litArea_naive, castRays, WallList
walls = WallList()
for w in range(1, 1000):
    walls.append(10, w, 200, w)
'''

    rays = timeit.Timer('castRays(50, 50, walls, 10000)', setup=setup)
    naive = timeit.Timer('litArea_naive(50, 50, walls)', setup=setup)
    simd = timeit.Timer('litArea(50, 50, walls)', setup=setup)

    repeat = 5

    with open('performance.txt', 'w') as fobj:

        w = Writer([sys.stdout, fobj])

        w.write(f'Start castRays (repeat={repeat}), take min = ')
        rayssec = minsec = min(rays.repeat(repeat=repeat, number=1))
        w.write(f'{minsec} seconds\n')

        w.write(f'Start litArea_naive (repeat={repeat}), take min = ')
        naivesec = minsec = min(naive.repeat(repeat=repeat, number=1))
        w.write(f'{minsec} seconds\n')

        w.write(f'Start litArea (repeat={repeat}), take min = ')
        simdsec = minsec = min(simd.repeat(repeat=repeat, number=1))
        w.write(f'{minsec} seconds\n')

        w.write('naive speed-up over rays: %g x\n' % (rayssec/naivesec))
        w.write('simd speed-up over naive: %g x\n' % (naivesec/simdsec))

if __name__ == '__main__':
    benchmark()