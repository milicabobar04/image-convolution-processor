#!/bin/bash

RESULTS_DIR="benchmark_results"
OUTPUT_DIR="benchmark_outputs" 

mkdir -p "$RESULTS_DIR"
mkdir -p "$OUTPUT_DIR"

RESULTS_FILE="$RESULTS_DIR/results.csv"

echo "optimization,threads,image_size,kernel_size,iteration,time_seconds" > "$RESULTS_FILE"

OPT_LEVELS=("O0" "O1" "O2" "O3")
THREAD_COUNTS=(1 2 4 8)
IMAGE_SIZES=(256 512 1024 2048)
KERNEL_SIZE=5
ITERATIONS=5
WARMUP_RUNS=2

compile_version() {
    local opt=$1
    local binary="convolution_${opt}"
    
    echo "Kompajliranje sa -${opt}..." >&2  
    gcc -${opt} -march=native -fopenmp bmp_parser.c convolution.c -o "$binary" -lm
    
    if [ $? -ne 0 ]; then
        echo "GRESKA: Kompajliranje nije uspjelo za -${opt}" >&2
        exit 1
    fi
    
    echo "$binary" 
}

generate_test_image() {
    local size=$1
    local filename=$2
    
    python3 - <<EOF
from PIL import Image
import numpy as np
img = np.random.randint(0, 256, ($size, $size, 3), dtype=np.uint8)
Image.fromarray(img, 'RGB').save('$filename')
EOF
}

run_benchmark() {
    # Definisemo parametre za testiranje
    local binary=$1
    local opt=$2
    local threads=$3
    local img_size=$4
    
    export OMP_NUM_THREADS=$threads
    
    # input slika se generise i spremamo output fajl
    local input_img="test_${img_size}.bmp"
    local output_img="${OUTPUT_DIR}/out_${opt}_${threads}t_${img_size}.bmp"
    
    if [ ! -f "$input_img" ]; then
        echo "Generisana slika: ${img_size}x${img_size}"
        generate_test_image $img_size "$input_img"
    fi
    
    # Box blur kernel
    local kernel_args="$KERNEL_SIZE"
    for i in $(seq 1 $((KERNEL_SIZE * KERNEL_SIZE))); do
        kernel_args="$kernel_args 1"
    done
    
    echo "Zagrijavanje ($opt, $threads niti, ${img_size}x${img_size})..."
    for i in $(seq 1 $WARMUP_RUNS); do
         ./"$binary" "$input_img" "$output_img" $kernel_args > /dev/null 2>&1
    done
    
    echo "Mjerenje ($opt, $threads niti, ${img_size}x${img_size})..."
    for iter in $(seq 1 $ITERATIONS); do
        local temp_output=$(mktemp)
        ./"$binary" "$input_img" "$output_img" $kernel_args > "$temp_output" 2>&1
        
        # Parsiranje vremena
        local time=$(grep "Vrijeme obrade:" "$temp_output" | grep -oP '\d+\.\d+' | head -1)
        
        if [ -z "$time" ]; then
            time=$(awk '/Vrijeme obrade:/ {print $3}' "$temp_output")
        fi
        
        if [ -n "$time" ]; then
            echo "$opt,$threads,$img_size,$KERNEL_SIZE,$iter,$time" >> "$RESULTS_FILE"
            echo "  Iteracija $iter: ${time}s"
        else
            echo "  Iteracija $iter: Nije pronadjeno vrijeme"
        fi
        
        rm -f "$temp_output"
    done
}

echo "=== BENCHMARK DISKRETNE KONVOLUCIJE ==="
echo ""

for opt in "${OPT_LEVELS[@]}"; do
    binary=$(compile_version "$opt")
    
    for img_size in "${IMAGE_SIZES[@]}"; do
        for threads in "${THREAD_COUNTS[@]}"; do
            run_benchmark "$binary" "$opt" "$threads" "$img_size"
        done
    done
    
    echo ""
done

echo "=== Generisanje grafika ==="
python3 - <<'PYTHON_SCRIPT'
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os

results_file = 'benchmark_results/results.csv'

if not os.path.exists(results_file):
    print("GRESKA: Nema rezultata za analizu!")
    exit(1)

df = pd.read_csv(results_file)

if df.empty:
    print("GRESKA: CSV fajl je prazan!")
    exit(1)

print(f"Ucitano {len(df)} mjerenja")

df_summary = df.groupby(['optimization', 'threads', 'image_size'])['time_seconds'].agg(['mean', 'std']).reset_index()

fig, axes = plt.subplots(2, 2, figsize=(16, 12))
fig.suptitle('Analiza Performansi Diskretne Konvolucije', fontsize=16, fontweight='bold')

# Graf 1: Optimizacioni nivoi
ax1 = axes[0, 0]
for threads in sorted(df_summary['threads'].unique()):
    data = df_summary[(df_summary['threads'] == threads) & (df_summary['image_size'] == 1024)]
    if not data.empty:
        ax1.plot(data['optimization'], data['mean'], marker='o', label=f'{threads} niti', linewidth=2)

ax1.set_xlabel('Nivo Optimizacije', fontweight='bold')
ax1.set_ylabel('Vrijeme (s)', fontweight='bold')
ax1.set_title('Uticaj Kompajlerskih Optimizacija (1024x1024)')
ax1.legend()
ax1.grid(True, alpha=0.3)

# Graf 2: Skalabilnost sa nitima
ax2 = axes[0, 1]
for opt in ['O0', 'O3']:
    data = df_summary[(df_summary['optimization'] == opt) & (df_summary['image_size'] == 1024)]
    if not data.empty:
        ax2.plot(data['threads'], data['mean'], marker='s', label=opt, linewidth=2)

ax2.set_xlabel('Broj Niti', fontweight='bold')
ax2.set_ylabel('Vrijeme (s)', fontweight='bold')
ax2.set_title('Skalabilnost Paralelizacije (1024x1024)')
ax2.legend()
ax2.grid(True, alpha=0.3)

# Graf 3: Ubrzanje (Speedup)
ax3 = axes[1, 0]
for opt in ['O0', 'O2', 'O3']:
    data = df_summary[(df_summary['optimization'] == opt) & (df_summary['image_size'] == 1024)]
    if not data.empty and len(data[data['threads'] == 1]) > 0:
        baseline = data[data['threads'] == 1]['mean'].values[0]
        speedup = baseline / data['mean']
        ax3.plot(data['threads'], speedup, marker='^', label=opt, linewidth=2)

if len(df_summary['threads'].unique()) > 0:
    ideal = sorted(df_summary['threads'].unique())
    ax3.plot(ideal, ideal, 'k--', label='Idealno', alpha=0.5)
    ax3.set_xticks(ideal)

ax3.set_xlabel('Broj Niti', fontweight='bold')
ax3.set_ylabel('Ubrzanje (Speedup)', fontweight='bold')
ax3.set_title('Ubrzanje Relativno na 1 Nit')
ax3.legend()
ax3.grid(True, alpha=0.3)

# Graf 4: Zavisnost od velicine slike
ax4 = axes[1, 1]
for threads in [1, 2, 4, 8]:
    data = df_summary[(df_summary['optimization'] == 'O3') & (df_summary['threads'] == threads)]
    if not data.empty:
        ax4.plot(data['image_size'], data['mean'], marker='o', label=f'{threads} niti', linewidth=2)

ax4.set_xlabel('Velicina Slike (pikseli)', fontweight='bold')
ax4.set_ylabel('Vrijeme (s)', fontweight='bold')
ax4.set_title('Skaliranje sa Velicinom Ulaza (O3)')
ax4.legend()
ax4.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('benchmark_results/performance_analysis.png', dpi=300, bbox_inches='tight')
print("Grafik sacuvan: benchmark_results/performance_analysis.png")

print("\n=== STATISTICKI IZVJESTAJ ===\n")
print(df_summary.to_string(index=False))

if not df_summary.empty:
    best = df_summary.loc[df_summary['mean'].idxmin()]
    print(f"\nNajbolje performanse:")
    print(f"  Optimizacija: {best['optimization']}")
    print(f"  Broj niti: {int(best['threads'])}")
    print(f"  Velicina: {int(best['image_size'])}x{int(best['image_size'])}")
    print(f"  Vrijeme: {best['mean']:.4f}s ± {best['std']:.4f}s")

PYTHON_SCRIPT

echo ""
echo "=== BENCHMARK ZAVRSEN ==="
echo "Rezultati sacuvani u: $RESULTS_FILE"
echo "Grafici sacuvani u: $RESULTS_DIR/performance_analysis.png"