import json
import argparse
import random

def generate_benchmark_config(num_blocks, output_file):
    """
    Generates a PLC configuration with a specified number of blocks.
    Creates a long chain of ADD blocks to simulate heavy logic.
    """
    config = {
        "program": {
            "name": "Benchmark_Program",
            "cycle_time": 100,
            "blocks": []
        }
    }

    # Create a chain of ADD blocks
    # Result = 0 + 1 + 1 + 1 ...
    
    # Initial block
    config["program"]["blocks"].append({
        "id": "block_0",
        "type": "ADD",
        "inputs": {
            "IN1": {"type": "const", "value": 0},
            "IN2": {"type": "const", "value": 1}
        },
        "outputs": {
            "OUT": "res_0"
        }
    })

    for i in range(1, num_blocks):
        config["program"]["blocks"].append({
            "id": f"block_{i}",
            "type": "ADD",
            "inputs": {
                "IN1": {"type": "var", "value": f"res_{i-1}"},
                "IN2": {"type": "const", "value": 1}
            },
            "outputs": {
                "OUT": f"res_{i}"
            }
        })

    with open(output_file, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"Generated benchmark configuration with {num_blocks} blocks in {output_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate PLC Benchmark Configuration")
    parser.add_argument("--blocks", type=int, default=100, help="Number of blocks to generate")
    parser.add_argument("--output", type=str, default="benchmark_config.json", help="Output JSON file")
    
    args = parser.parse_args()
    generate_benchmark_config(args.blocks, args.output)
