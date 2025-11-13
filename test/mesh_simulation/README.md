# Mesh Network Simulation Plan

## Objective
The primary objective of this simulation is to analyze the performance, stability, and theoretical limitations of the `painlessMesh` network used in the EspHub project. This analysis will be conducted in a virtual environment without the need for physical hardware, allowing for rapid testing of various complex scenarios.

## Methodology
A discrete-event simulation will be developed using Python. The simulation will model the core concepts of the `painlessMesh` library, focusing on:
- **Nodes:** Each node in the mesh will be an independent entity in the simulation.
- **Network Topology:** The simulation will replicate the dynamic tree structure of the mesh.
- **Messaging:** Both broadcast and unicast messaging will be modeled, including message propagation through the tree.
- **Time:** The simulation will manage a virtual clock to measure time-dependent events like packet latency and network reorganization time.

The simulation will abstract the low-level details of the radio layer and focus on the logical behavior of the mesh protocol.

## Tools
- **Python 3:** The core language for the simulation logic.
- **SimPy:** A process-based discrete-event simulation framework. It will be used to manage the simulation clock, events, and the concurrent processes of the nodes.
- **NetworkX:** For creating, manipulating, and studying the structure and dynamics of the network graph.
- **Matplotlib / Seaborn:** For visualizing the simulation results, such as charts for latency, node load, and queue lengths over time.
- **Jupyter Notebooks (Optional):** May be used for interactive exploration, running simulations, and visualizing results.

## Core Simulation Components
- **`Node` Class:** Represents a single ESP32 device. It will have properties such as `nodeId`, a list of direct connections (`neighbors`), a message queue, and metrics for tracking its load. Each node will run as a separate process in the SimPy environment.
- **`Message` Class:** Represents a data packet. It will contain information like `source_id`, `destination_id`, `payload_size`, and creation `timestamp` to calculate latency.
- **`Network` Class:** A manager class that orchestrates the simulation. It will be responsible for creating nodes, defining the network topology, running the simulation for a specified duration, and collecting global metrics.