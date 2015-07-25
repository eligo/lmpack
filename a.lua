local mp = require("lmpack")

local data = {
        name = "Alice",
        id = 10000,
        email = "Alice@qq.com",
        phone = {
            { number = "123456789" , type = 1 },
            { number = "87654321" , type = 2 },
        }
}

local p = mp.pack(data)
local stm = os.clock()
for i = 1, 1000000 do
	mp.unpack(p)
end
print("cost", os.clock() - stm)