# Mesh Network Simulation

## Objective
The primary objective of this simulation is to analyze the performance, stability, and theoretical limitations of the `painlessMesh` network used in the EspHub project. This analysis will be conducted in a virtual environment without the need for physical hardware, allowing for rapid testing of various complex scenarios.

## Methodology
A discrete-event simulation developed using Python models the core concepts of the `painlessMesh` library, focusing on:
- **Nodes:** Each node in the mesh is an independent entity in the simulation
- **Network Topology:** The simulation replicates the dynamic tree structure of the mesh
- **Messaging:** Both broadcast and unicast messaging are modeled, including message propagation through the tree
- **Time:** The simulation manages a virtual clock to measure time-dependent events like packet latency and network reorganization time

The simulation abstracts the low-level details of the radio layer and focuses on the logical behavior of the mesh protocol.

## Tools & Technologies
- **Python 3.8+** - Core language for simulation logic
- **SimPy** - Process-based discrete-event simulation framework
- **NetworkX** - Network graph creation and analysis
- **Matplotlib / Seaborn** - Visualization of simulation results
- **Pandas / NumPy** - Data analysis and manipulation

## Installation

### 1. Setup Virtual Environment (Windows)
```batch
setup.bat
```

This will:
- Create a Python virtual environment
- Install all required dependencies
- Activate the environment

### 2. Manual Installation
```batch
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

## Project Structure

```
mesh_simulation/
├── message.py          - Message/packet representation
├── node.py            - Node (ESP32 device) implementation
├── network.py         - Network manager and simulation orchestrator
├── metrics.py         - Metrics collection and analysis
├── scenarios.py       - 7 test scenarios implementation
├── visualization.py   - Advanced visualization tools
├── main.py           - Main entry point
├── requirements.txt  - Python dependencies
├── setup.bat         - Windows setup script
├── results/          - Output directory (auto-created)
├── README.md         - This file
├── SCENARIOS.md      - Detailed scenario descriptions
└── METRICS.md        - Metrics definitions
```

## Core Components

### Message Class
Represents a data packet with source, destination, payload, and routing information. Tracks hop count, path, and delivery metrics.

### Node Class
Represents a single ESP32 device with:
- Network topology (parent, children, neighbors)
- Message queue with processing
- Metrics collection (load, queue length, processing time)
- Tree-based routing logic

### Network Class
Orchestrates the simulation:
- Creates and manages nodes
- Builds mesh tree topology
- Handles node failures and restructuring
- Collects global metrics and statistics

## Usage

### Run All Scenarios
```batch
python main.py --all
```

### Run Specific Scenario
```batch
# Scenario 1: Baseline Performance
python main.py --scenario 1

# Scenario 3: Root Node Failure
python main.py --scenario 3

# Scenario 5: Scalability (custom max nodes)
python main.py --scenario 5 --nodes 50
```

### Run with Custom Parameters
```batch
# Custom number of nodes
python main.py --scenario 2 --nodes 20

# Custom duration
python main.py --scenario 1 --duration 600
```

## Available Scenarios

1. **Baseline Performance** - Low traffic, stable network
2. **High Traffic / Broadcast Storm** - Heavy load testing
3. **Root Node Failure** - Network restructuring analysis
4. **Intermediate Node Failure** - Bridge node failure handling
5. **Network Scalability** - Performance vs network size
6. **Single-Node Bottleneck** - Target node overload testing
7. **Network Partition** - Split-brain scenario

See [SCENARIOS.md](SCENARIOS.md) for detailed descriptions.

## Metrics Collected

See [METRICS.md](METRICS.md) for complete definitions. Key metrics include:

- **End-to-End Latency** (avg, min, max, P95)
- **Node Load** (messages sent/received/forwarded)
- **Message Queue Length**
- **Packet Loss Rate**
- **Network Restructuring Time**
- **Network Throughput**

## Output & Results

All results are saved to the `results/` directory:

### JSON Results
```
results/Scenario_X_YYYYMMDD_HHMMSS.json
```
Complete simulation data in JSON format.

### Markdown Reports
```
results/Scenario_X_report.md
```
Human-readable reports with analysis and recommendations.

### Visualizations
- `*_latency.png` - Latency distribution histograms
- `*_load.png` - Node load bar charts
- `*_heatmap.png` - Node activity heatmaps
- `all_scenarios_comparison.png` - Cross-scenario comparison
- `scalability_analysis.png` - Scalability results

## Visualization

### Generate All Visualizations
```batch
python visualization.py --all
```

### Specific Visualizations
```batch
# Comparison across scenarios
python visualization.py --compare

# Scalability analysis
python visualization.py --scalability

# Node activity heatmap
python visualization.py --heatmap Scenario_1_Baseline
```

## Example Workflow

```batch
# 1. Setup environment
setup.bat

# 2. Activate environment
venv\Scripts\activate

# 3. Run all scenarios
python main.py --all

# 4. Generate visualizations
python visualization.py --all

# 5. Review results
dir results\
```

## Interpreting Results

### Good Network Health
- Packet loss < 1%
- Average latency < 100ms
- No growing queue lengths
- Balanced node load

### Warning Signs
- Packet loss 1-10%
- Average latency 100-1000ms
- Periodic queue spikes
- Root node heavily loaded

### Critical Issues
- Packet loss > 10%
- Average latency > 1000ms
- Continuously growing queues
- Frequent node failures

## Recommendations Based on Results

Reports automatically include recommendations for:
- Network capacity limits
- Optimal node count
- Traffic management strategies
- Topology improvements
- Code optimization suggestions

## Next Steps

1. Run all scenarios to establish baseline
2. Review reports and identify bottlenecks
3. Experiment with different topologies
4. Test edge cases and failure modes
5. Document findings and recommendations
6. Apply insights to EspHub implementation

## Troubleshooting

**Import errors:** Ensure virtual environment is activated
**No results:** Check `results/` directory exists (auto-created on first run)
**Plot errors:** Verify matplotlib backend is properly configured

## Contributing

When adding new scenarios:
1. Inherit from `Scenario` base class
2. Implement `setup()` and `run()` methods
3. Collect metrics using `MetricsCollector`
4. Add to scenario map in `main.py`

## License

Part of the EspHub project testing suite.