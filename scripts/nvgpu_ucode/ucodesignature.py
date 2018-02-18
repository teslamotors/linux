import sys, struct
headersize = 16
sizeof_u32 = 4
ucode = open(sys.argv[1]).read()[headersize:]
assert struct.calcsize("I") == sizeof_u32
fmt = "I" * (len(ucode) / sizeof_u32)
ints = struct.unpack(fmt, ucode)
print len(ucode), "bytes"
print "sig =", hex(sum(ints) & 0xffffffff)
