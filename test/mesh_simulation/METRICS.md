# Simulation Metrics

This document defines the key performance indicators (KPIs) and metrics that will be collected and analyzed during the mesh network simulation.

### 1. End-to-End Latency
- **Description:** The time elapsed from the moment a message is created at the source node until it is successfully received by its final destination node.
- **Unit:** Milliseconds (ms).
- **Statistics:**
    - Average
    - Minimum
    - Maximum
    - 95th Percentile (P95)
    - Standard Deviation
- **Analysis:** This metric is crucial for understanding the responsiveness of the network. High latency can be a problem for time-sensitive applications.

### 2. Node Load
- **Description:** An abstract, calculated value representing the amount of work a node is performing. It can be a weighted sum of operations performed per unit of time.
- **Calculation (Example):** `Load = (w1 * messages_sent) + (w2 * messages_received) + (w3 * messages_forwarded)`
- **Unit:** Abstract "Load Units" per second.
- **Analysis:** Helps identify which nodes are under the most stress (typically the root and nodes with many children). This is key for understanding network bottlenecks.

### 3. Message Queue Length
- **Description:** The number of messages currently waiting in a node's input queue to be processed.
- **Unit:** Integer (number of messages).
- **Statistics:**
    - Average queue length over time.
    - Maximum queue length reached.
- **Analysis:** A consistently growing queue indicates that a node cannot process messages as fast as it receives them, which is a sign of a bottleneck and can lead to increased latency and packet loss.

### 4. Packet Loss Rate
- **Description:** The percentage of messages that were sent but never reached their intended destination.
- **Unit:** Percentage (%).
- **Calculation:** `(total_messages_lost / total_messages_sent) * 100`
- **Analysis:** This is a critical indicator of network instability. Packet loss can occur during network restructuring (e.g., node failure) or due to queue overflows on overloaded nodes.

### 5. Network Restructuring Time
- **Description:** The time it takes for the mesh to form a new, stable tree topology after a disruptive event, such as a node failure.
- **Unit:** Seconds (s).
- **Calculation:** The timestamp of the "new tree stable" event minus the timestamp of the "node failure" event.
- **Analysis:** A long restructuring time means the network is unavailable for a longer period, leading to more significant packet loss.

### 6. Network Throughput
- **Description:** The total amount of application data (payload) successfully delivered across the network per unit of time.
- **Unit:** Kilobits per second (kbps).
- **Calculation:** `(Sum of all payload sizes of delivered messages) / (Total simulation time)`
- **Analysis:** Measures the overall data-carrying capacity of the network under specific conditions.
