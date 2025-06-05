#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <test_size> <injector_size>"
    exit 1
fi

test_size=$1
injector_size=$2
timestamp=$(date +"%Y%m%d_%H%M%S")

combined_log="combined_${timestamp}.log"

c_pipe="/tmp/c_pipe_$$"
py_pipe="/tmp/py_pipe_$$"
mkfifo "$c_pipe" "$py_pipe"

./fat32_test "$test_size" 2>&1 | while IFS= read -r line; do
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] [C] $line"
done > "$c_pipe" &
c_pid=$!

python3 injector.py --count "$injector_size" 2>&1 | while IFS= read -r line; do
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] [PY] $line"
done > "$py_pipe" &
py_pid=$!

tee "$combined_log" < <(cat "$c_pipe" "$py_pipe") >/dev/null &
tee_pid=$!

wait "$c_pid"
c_exit=$?

if kill -0 "$py_pid" 2>/dev/null; then
    kill "$py_pid" >/dev/null 2>&1
    wait "$py_pid" 2>/dev/null
fi

rm -f "$c_pipe" "$py_pipe"
kill "$tee_pid" 2>/dev/null
wait "$tee_pid" 2>/dev/null

echo "Test complete:"
echo "Combined log: $combined_log"
echo "C return code: $c_exit"