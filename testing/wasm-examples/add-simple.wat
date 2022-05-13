(module
 ;; Import the Fixpoint API
 ;;  Attach a Fixpoint Tree to a Wasm table:
 (import "fixpoint" "attach_tree_ro_table_0" (func $attach_tree_ro_table_0 (param externref)))
 ;;  Attach a Fixpoint Blob to a Wasm memory:
 (import "fixpoint" "attach_blob_ro_mem_0"   (func $attach_blob_ro_mem_0 (param externref)))
 (import "fixpoint" "attach_blob_ro_mem_1"   (func $attach_blob_ro_mem_1 (param externref)))
 ;;  Detach a Wasm memory (returns reference to a Fixpoint MBlob):
 (import "fixpoint" "create_blob_rw_mem_0"    (func $create_blob_rw_mem_0 (param i32)(result externref)))

 ;; Declare a read-only table to hold the ENCODE
 (table $ro_table_0 (export "ro_table_0") 0 externref)

 ;; Declare two read-only memories to hold the addend Blobs
 (memory $ro_mem_0  (export "ro_mem_0")   0)
 (memory $ro_mem_1  (export "ro_mem_1")   0)

 ;; Declare a read-write memory for the output Blob
 (memory $rw_mem_0  (export "rw_mem_0")   1)

 ;; Code for the add function, whose one parameter is an ENCODE with four elements:
 ;;  #0: resource limits, #1: this Wasm module, #2/#3: the addends (each a 4-byte Blob representing a 32-bit integer)

 (func (export "_fixpoint_apply") (param $encode externref) (result externref)
       (call $attach_tree_ro_table_0 (local.get $encode))                 ;; attach the ENCODE to $ro_table_0

       (call $attach_blob_ro_mem_0 (table.get $ro_table_0 (i32.const 2))) ;; attach 1st addend to $ro_mem_0
       (call $attach_blob_ro_mem_1 (table.get $ro_table_0 (i32.const 3))) ;; attach 2nd addend to $ro_mem_1

       (i32.store $rw_mem_0                                               ;; Store to the output memory...
		   (i32.const 0)                                           ;; ... at location zero...
		   (i32.add (i32.load $ro_mem_0 (i32.const 0))             ;; ... the sum of the two addends.
			   (i32.load $ro_mem_1 (i32.const 0))))

       (call $create_blob_rw_mem_0                             ;; Freeze the output Blob from the output memory...
	     (i32.const 4))))                                        ;; ... and a length of 4 bytes.
