local bytedata = Thorium.data.newBytedata(16)

for i = 0, 15 do
  bytedata:setUInt8(i, i * 2)
end

for i = 0, 15 do
  local value = bytedata:getUInt8(i, 1)
  assert(value == i * 2, "ByteData value mismatch at index " .. i .. ": " .. value)
end

assert(bytedata:getSize() == 16, "ByteData size mismatch: " .. bytedata:getSize())
assert(pcall(bytedata.getUInt8, bytedata, 16) == false, "Expected error for out-of-bounds access")
print("ByteData tests passed.")
