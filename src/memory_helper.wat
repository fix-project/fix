(module
  (import "env" "memory" (memory $tmem 0))
  (memory $mymem (export "rw_mem_0") 1)
  (func (export "memory_copy_rw_0") (param $ptr i32) (param $len i32)
  	(memory.copy $tmem $mymem
		(i32.const 0)
		(local.get $ptr)
		(local.get $len)
	)
  )
)

