#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <test_size> <injector_size>"
    exit 1
fi

test_size=$1
injector_size=$2
timestamp=$(date +"%Y%m%d_%H%M%S")

c_log="c_program_${timestamp}.log"
py_log="python_script_${timestamp}.log"

./fat32_test "$test_size" > "$c_log" 2>&1 &
c_pid=$!

python3 injector.py --count "$injector_size" > "$py_log" 2>&1 &
py_pid=$!

wait "$c_pid"
c_exit=$?

if kill -0 "$py_pid" 2>/dev/null; then
    kill "$py_pid" >/dev/null 2>&1
    wait "$py_pid" 2>/dev/null
fi

echo "Test complete:"
echo "C-test: $c_log"
echo "Injector: $py_log"
echo "C return code: $c_exit"
