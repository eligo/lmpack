local mp = require("lmpack")

local pa = mp.create()
local pb = mp.create()
pb:pack("hello1")
print(pa)
pa:pack(11, "hello2dddddddddddd", 0, 33888, "8888888888888888888888888888888a")
print(pa:unpack(5))