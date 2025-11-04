function script()
  -- Set SMB1 lives to 99 (stored as 98 in memory)
  writebyte(0x075A, 98)
end

