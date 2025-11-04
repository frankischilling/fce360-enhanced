function script()
  -- Always keep Mario as Fire Mario
  writebyte(0x0756, 2)  -- 0=Small, 1=Super, 2=Fire
end

