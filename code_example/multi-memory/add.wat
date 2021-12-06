(module
  (type (;0;) (func (param i32 i32) ))
  (memory $memone 1)
  (memory $memtwo 2)
  (memory $memthree 3)
  (memory $memfour 4)
  (func $_start (type 0) (param i32 i32) 
    i32.const 0 
    i32.const 42
    i32.store offset=0)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 5)
  (global (;0;) (mut i32) (i32.const 66560))
  (export "mem" (memory 0))
  (export "_start" (func $_start)))
