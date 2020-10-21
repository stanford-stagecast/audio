import sys
import matplotlib.pyplot as plt

if __name__ == "__main__":
    filename = sys.argv[1]

    min_buf = float('inf')
    with open(filename, 'r') as f:
        buffers = [float(line) for line in f]
        # for i, line in enumerate(f):
        #     # if i % 10000 == 0:
        #     #     print("reading line {}".format(i))
        #     buf = float(line)
        #     if buf < min_buf:
        #         # print("new min buf: {}".format(buf))
        #         min_buf = buf
        
    print("minimum buffer: {}".format(min(buffers)))
    plt.plot(buffers)
    plt.title("Buffer over 3M packets, wired")
    plt.xlabel("Packet number")
    plt.ylabel("Buffer value (ms)")
    plt.savefig('/home/jchen18/audio/figures/buffer3M_wired.png')