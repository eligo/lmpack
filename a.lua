local mp = require("lmpack")

local stm = os.clock()
for i=1, 1000000 do
	local pa = mp.create()
	local pb = mp.create()
	pb:pack("hello1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab")
	pa:pack(pb, 1111111,12222222222,1333333333,"wwwwwwwwwwwwwwwwwwd")
	--local a, b,c,d,e= pa:unpack(5)
	--print(a,b,c,d,e)
	--print(a:unpack(2))
	--break
end
print(os.clock() - stm)
--local t1, t2 = pa:unpack(2)
--print(t1, t2)
--print(t2:unpack(1))