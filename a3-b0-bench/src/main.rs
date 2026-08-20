use std::env;
use std::hint::black_box;
use std::time::Instant;

use miniz_oxide::deflate::compress_to_vec_zlib;
use miniz_oxide::inflate::decompress_to_vec_zlib;

const INPUT_BYTES: usize = 4 * 1024;
const DEFAULT_ITERATIONS: u64 = 1_000;

fn make_input() -> Vec<u8> {
    const PHRASE: &[u8] =
        b"CapsLock A3 fixed zlib-rs workload: capability checking versus emulation.\n";

    (0..INPUT_BYTES)
        .map(|i| {
            if i % 257 < 16 {
                ((i.wrapping_mul(31) + i / 257) & 0xff) as u8
            } else {
                PHRASE[i % PHRASE.len()]
            }
        })
        .collect()
}

fn main() {
    let iterations = env::args()
        .nth(1)
        .map(|value| value.parse::<u64>().expect("iterations must be an integer"))
        .unwrap_or(DEFAULT_ITERATIONS);
    assert!(iterations > 0, "iterations must be greater than zero");

    let input = make_input();
    let compressed = compress_to_vec_zlib(&input, 6);

    // Verify the fixed input and warm up the decompressor before timing.
    let warmup = decompress_to_vec_zlib(&compressed).expect("warm-up decompression failed");
    assert_eq!(warmup, input, "warm-up output differs from the fixed input");
    drop(warmup);

    let mut checksum = 0u64;
    let start = Instant::now();

    for iteration in 0..iterations {
        let output = decompress_to_vec_zlib(black_box(&compressed))
            .expect("timed decompression failed");
        assert_eq!(output.len(), INPUT_BYTES);

        let first = output[(iteration as usize * 997) % output.len()] as u64;
        let second = output[(iteration as usize * 7919 + 17) % output.len()] as u64;
        checksum = checksum.rotate_left(7) ^ first ^ (second << 8) ^ output.len() as u64;
        black_box(&output);
    }

    let elapsed = start.elapsed();
    println!(
        "iterations={} input_bytes={} compressed_bytes={} output_bytes={} checksum={} elapsed_ns={}",
        iterations,
        input.len(),
        compressed.len(),
        input.len() * iterations as usize,
        checksum,
        elapsed.as_nanos()
    );
}
