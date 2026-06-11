#!/usr/bin/env python3

import numpy as np
from PIL import Image
import subprocess
import os

def create_test_image(size, pattern='gradient'):
    if pattern == 'gradient':
        img = np.zeros((size, size, 3), dtype=np.uint8) # kreiramo crnu sliku
        for i in range(size):
            img[i, :, :] = int(255 * i / size) # svi pikseli u redu dobijaju istu boju kako idemo dalje stvara se prelaz
    elif pattern == 'checkerboard':
        img = np.zeros((size, size, 3), dtype=np.uint8)
        block = 8 # 8x8 piskela je jedno polje sah tabe;
        for i in range(size):
            for j in range(size):
                if ((i // block) + (j // block)) % 2 == 0:
                    img[i, j] = [255, 255, 255] # bijela boja za parne blokove
    elif pattern == 'edges':
        img = np.ones((size, size, 3), dtype=np.uint8) * 128
        img[size//4:3*size//4, size//4:3*size//4] = [255, 255, 255] # sredisnji kvadrat velicine pola slike je bijeli
    else:
        img = np.random.randint(0, 256, (size, size, 3), dtype=np.uint8) # nasuumcine boje piskela
    
    return img

def manual_convolution(img, kernel):
    h, w = img.shape[:2] 
    ksize = kernel.shape[0] 
    krad = ksize // 2
    
    result = np.zeros_like(img, dtype=np.float32)
    
    for y in range(h):
        for x in range(w):
            for c in range(3):
                s = 0.0
                for ky in range(-krad, krad + 1):
                    for kx in range(-krad, krad + 1):
                        px = np.clip(x + kx, 0, w - 1)
                        py = np.clip(y + ky, 0, h - 1)
                        s += img[py, px, c] * kernel[ky + krad, kx + krad]
                result[y, x, c] = np.clip(s, 0, 255)
    
    return result.astype(np.uint8)

def run_c_convolution(input_path, output_path, kernel):
    ksize = kernel.shape[0]
    kernel_flat = kernel.flatten().tolist()
    kernel_args = [str(ksize)] + [str(k) for k in kernel_flat]
    
    cmd = ['./convolution_O3', input_path, output_path] + kernel_args
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Greska: {result.stderr}")
        return None
    
    return np.array(Image.open(output_path))

def compare_images(img1, img2, tolerance=2):
    diff = np.abs(img1.astype(float) - img2.astype(float)) # apsolutna razlika za svaki piksel
    max_diff = np.max(diff) # najveca apsolutna razlika u pikselima i kanalima
    mean_diff = np.mean(diff) # prosjecna razlika
    
    # dif <= tolerance vraca nam niz true/false i np.sum vraca broj piksela unutar toleranije i racunamo procenat
    pixels_within_tolerance = np.sum(diff <= tolerance) / diff.size * 100
    
    return {
        'max_diff': max_diff,
        'mean_diff': mean_diff,
        'accuracy': pixels_within_tolerance
    }

def test_kernel(name, kernel):
    print(f"\n=== Test: {name} ===")
    
    size = 64
    img = create_test_image(size, 'random')
    
    input_path = 'test_input.bmp'
    output_path = 'test_output.bmp'
    
    Image.fromarray(img).save(input_path)
    
    # Referentna implementacija
    expected = manual_convolution(img, kernel)
    
    # C implementacija
    result = run_c_convolution(input_path, output_path, kernel)
    
    if result is None:
        print("Greska pri izvrsavanju C koda")
        return False
    
    # Poredjenje
    comparison = compare_images(expected, result)
    
    print(f"Maksimalna razlika: {comparison['max_diff']:.2f}")
    print(f"Prosjecna razlika: {comparison['mean_diff']:.4f}")
    print(f"Tacnost: {comparison['accuracy']:.2f}%")
    
    if comparison['accuracy'] > 98.0:
        print("✓ Test PROSAO")
        return True
    else:
        print("Test NIJE PROSAO")
        return False

def main():
    print("=" * 60)
    print("TEST TACNOSTI ALGORITMA KONVOLUCIJE")
    print("=" * 60)
    
    kernels = {
        'Identitet (3x3)': np.array([
            [0, 0, 0],
            [0, 1, 0],
            [0, 0, 0]
        ], dtype=float),
        
        'Box Blur (3x3)': np.array([
            [1, 1, 1],
            [1, 1, 1],
            [1, 1, 1]
        ], dtype=float) / 9.0,
        
        'Sharpen (3x3)': np.array([
            [0, -1, 0],
            [-1, 5, -1],
            [0, -1, 0]
        ], dtype=float),
        
        'Edge Detection (3x3)': np.array([
            [-1, -1, -1],
            [-1, 8, -1],
            [-1, -1, -1]
        ], dtype=float),
        
        'Gaussian Blur (5x5)': np.array([
            [1, 4, 6, 4, 1],
            [4, 16, 24, 16, 4],
            [6, 24, 36, 24, 6],
            [4, 16, 24, 16, 4],
            [1, 4, 6, 4, 1]
        ], dtype=float) / 256.0
    }
    
    passed = 0
    total = len(kernels)
    
    for name, kernel in kernels.items():
        if test_kernel(name, kernel):
            passed += 1
    
    print("\n" + "=" * 60)
    print(f"REZULTATI: {passed}/{total} testova proslo")
    print("=" * 60)
    
    for f in ['test_input.bmp', 'test_output.bmp']:
        if os.path.exists(f):
            os.remove(f)
    
    return passed == total

if __name__ == '__main__':
    import sys
    sys.exit(0 if main() else 1)