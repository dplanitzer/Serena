# usage: finalizerom infile outfile

from sys import argv, exit
import array

romsize = 256*1024
autovec = [0, 24, 0, 25, 0, 26, 0, 27, 0, 28, 0, 29, 0, 30, 0, 31]

# read rom file

with open(argv[1], "rb") as f:
  rom = array.array('b')
  try:  rom.fromfile(f, romsize-len(autovec))
  except EOFError:  pass
  if len(f.read(1)) != 0:  exit("*** rom file too large")

# finalize rom and write to disk

filesize = rom.buffer_info()[1]
rom.extend([0]*(romsize-filesize-len(autovec)))
rom.extend(autovec)
with open(argv[2], "wb") as f:  rom.tofile(f)
