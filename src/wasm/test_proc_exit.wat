(module $test_proc_exit.wasm
  (type (;0;) (func (param i32)))
  (type (;1;) (func))
  (type (;2;) (func))
  (import "wasi_snapshot_preview1" "proc_exit" (func $__wasi_proc_exit (type 0)))
  (import "custom_embedded" "hello" (func $hello (type 1)))
  (func $_start (type 2)
    (local i32)
    i32.const 0
    (call $hello)
    (call $__wasi_proc_exit  (i32.const 10))
    unreachable
  )
  (memory (;0;) 1)
  (export "memory" (memory 0))
  (export "_start" (func $_start))
)