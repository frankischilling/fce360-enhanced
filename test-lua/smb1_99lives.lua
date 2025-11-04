function script()
  -- SMB1 stores lives as (displayed_lives - 1)
  -- To show 99 lives, write 98 to 0x075A
  writebyte(0x075A, 98)
end

