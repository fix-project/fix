(module
 ;; Import the Fixpoint API
 ;;  Attach a Fixpoint Tree to a Wasm table:
 ;;  Detach a Wasm memory (returns reference to a Fixpoint MBlob):
 (import "fixpoint" "detach_mem_rw_mem_0"    (func $detach_mem_rw_mem_0 (result externref)))
 ;;  Freeze an MBlob into a Blob of a given length:
 (import "fixpoint" "freeze_blob"            (func $freeze_blob (param externref i32) (result externref)))

 ;; Declare a read-write memory for the output Blob
 (memory $rw_mem_0  (export "rw_mem_0")   1)

 ;; Code for the add function, whose one parameter is an ENCODE with four elements:
 ;;  #0: resource limits, #1: this Wasm module, #2/#3: the addends (each a 4-byte Blob representing a 32-bit integer)

 (func (export "_fixpoint_apply") (param $encode externref) (result externref)
       (i32.store $rw_mem_0
		  (i32.const 0)
		  (i32.const 3))

       (call $freeze_blob                                                 ;; Freeze the output Blob...
	     (call $detach_mem_rw_mem_0)                                  ;; ... with contents from the output memory...
	     (i32.const 4))))                                             ;; ... and a length of 4 bytes.
