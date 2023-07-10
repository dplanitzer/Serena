# usage: finalizerom inKernelBinFile inTestsBinFile outRomFile

from sys import argv, exit
import array

romsize = 256*1024
autovec = [0, 24, 0, 25, 0, 26, 0, 27, 0, 28, 0, 29, 0, 30, 0, 31]
payloadsize = romsize - len(autovec)


# read the kernel rom file
with open(argv[1], "rb") as f:
  kernel_bin = array.array('b')
  try:  kernel_bin.fromfile(f, payloadsize//2)
  except EOFError:  pass
  if len(f.read(1)) != 0:  exit("*** kernel bin file is too big")


# read the kernel test rom file
with open(argv[2], "rb") as f:
  tests_bin = array.array('b')
  try:  tests_bin.fromfile(f, payloadsize//2)
  except EOFError:  pass
  if len(f.read(1)) != 0:  exit("*** kernel tests bin file is too big")


# finalize rom and write to disk
kernel_bin_size = kernel_bin.buffer_info()[1]
tests_bin_size = tests_bin.buffer_info()[1]

rom = array.array('b')
rom.extend(kernel_bin)
rom.extend([0]*(romsize//2-kernel_bin_size))
rom.extend(tests_bin)
rom.extend([0]*(romsize//2-tests_bin_size-len(autovec)))
rom.extend(autovec)
with open(argv[3], "wb") as f:  rom.tofile(f)

#rom.extend([0]*(romsize-filesize-len(autovec)))
