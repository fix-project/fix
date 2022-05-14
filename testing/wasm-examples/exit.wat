(module
 ;; Import the Fixpoint API
 (import "fixpoint" "create_blob_rw_mem_0"   (func $create_blob_rw_mem_0 (param i32) (result externref)))
 (import "fixpoint" "create_blob_rw_mem_1"   (func $create_blob_rw_mem_1 (param i32) (result externref)))
 (import "fixpoint" "exit"		     (func $exit (param externref)))

 (memory $rw_mem_0  (export "rw_mem_0") 1)
 (memory $rw_mem_1  (export "rw_mem_1") 1)

 (func (export "_fixpoint_apply") (param $encode externref) (result externref)
       (i32.store $rw_mem_0
		  (i32.const 0)
		  (i32.const 17))

       (i32.store $rw_mem_1
		  (i32.const 0)
		  (i32.const 42))

       (call $exit (call $create_blob_rw_mem_0
			 (i32.const 4)))
       
       (call $create_blob_rw_mem_1 (i32.const 4))
))
