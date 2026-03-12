if [ "$#" -lt 6 ]; then
  echo "Usage: $0 <bitflips_start> <bitflips_end> <bitflips_step> <size_start_mb> <size_end_mb> <size_step_mb> [python_script] [output_dir] [extra python args...]"
  exit 1
fi

bitflips_start="$1"
bitflips_end="$2"
bitflips_step="$3"
size_start_mb="$4"
size_end_mb="$5"
size_step_mb="$6"
python_script="${7:-./bitflip_tester.py}"
output_dir="${8:-./results}"

shift $(( $# >= 8 ? 8 : $# ))
extra_args=("$@")

if [ "$bitflips_step" -le 0 ] || [ "$size_step_mb" -le 0 ]; then
  echo "Error: steps must be greater than 0"
  exit 1
fi

mkdir -p "$output_dir"

log() {
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
}

for ((bitflips=bitflips_start; bitflips<=bitflips_end; bitflips+=bitflips_step)); do
  for ((size=size_start_mb; size<=size_end_mb; size+=size_step_mb)); do
    result_file="$output_dir/result_${bitflips}_${size}.txt"
    image_file="nifat32.img"

    log "Running test: bitflips=$bitflips, image_size=${size}MB"
    python3 "$python_script" \
      "${extra_args[@]}" \
      --bitflips-count "$bitflips" \
      --image-size "$size" \
      --image-path "$image_file" \
      --stats-output "$result_file"

    log "Saved result: $result_file"
  done
done

log "Done"