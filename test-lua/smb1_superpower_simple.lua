function script()
  -- Always keep Mario as Super Mario
  writebyte(0x0756, 1)  -- 0=Small, 1=Super, 2=Fire
end

