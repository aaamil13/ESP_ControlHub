# Mesh Simulation Scenarios

This document outlines the specific theoretical scenarios that will be simulated to test the behavior of the mesh network.

### Scenario 1: Baseline Performance
- **Description:** A stable network of a fixed number of nodes (e.g., 15 nodes) is established. Communication is infrequent, with nodes sending a broadcast message at a low, steady rate (e.g., one message per node every 60 seconds).
- **Goal:** Establish baseline metrics for latency and node load in a low-traffic environment. This will serve as a control for other scenarios.

### Scenario 2: High Traffic / Broadcast Storm
- **Description:** All nodes in the network begin broadcasting messages at a high frequency (e.g., one message per node every 1-5 seconds).
- **Goal:** Measure the impact of high traffic on end-to-end latency, message queue lengths at each node, and the load on the root node and intermediate nodes. This simulates a "broadcast storm" condition.

### Scenario 3: Root Node Failure
- **Description:** The simulation runs until a stable network is formed. Then, the root node is abruptly removed from the simulation.
- **Goal:** Observe and measure the time it takes for the network to restructure and elect a new root node. Key metrics include the "network down" time (time until a new stable tree is formed) and the rate of packet loss during the transition.

### Scenario 4: Intermediate Node Failure
- **Description:** A node that connects two or more sub-trees (a bridge node) is removed from the simulation.
- **Goal:** Analyze the behavior of the orphaned children of the failed node. Measure the time it takes for them to find new parents and reconnect to the mesh.

### Scenario 5: Network Scalability
- **Description:** The simulation will be run in iterations. Starting with a small number of nodes (e.g., 5), each iteration will add a set number of new nodes (e.g., +5 nodes) up to a maximum (e.g., 100 nodes).
- **Goal:** Measure how performance metrics (average latency, root node load, network formation time) change as the network size increases. This will help determine the practical scalability limits of the mesh.

### Scenario 6: Single-Node Bottleneck
- **Description:** In a stable network, all nodes begin sending unicast messages to a single, non-root target node at a high rate.
- **Goal:** Measure the processing load and message queue length of the target "bottleneck" node. This simulates a scenario where one device (e.g., a central controller) is the target of many sensor readings.

### Scenario 7: Network Partition (Split-Brain)
- **Description:** Simulate a condition (e.g., failure of multiple intermediate nodes) that causes the network to split into two or more independent, disconnected sub-meshes.
- **Goal:** Observe the behavior of the partitioned sub-meshes. Do they each elect their own root node? What happens when the connectivity is restored?
