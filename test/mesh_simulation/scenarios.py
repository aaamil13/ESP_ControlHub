"""
Scenario implementations for mesh network simulation.
Each scenario tests specific aspects of the mesh network behavior.
"""

import simpy
from network import Network
from metrics import MetricsCollector


class Scenario:
    """Base class for simulation scenarios."""

    def __init__(self, name, description):
        self.name = name
        self.description = description
        self.network = None
        self.metrics = None
        self.params = {}

    def setup(self):
        """Setup the scenario. Override in subclasses."""
        pass

    def run(self):
        """Run the scenario. Override in subclasses."""
        pass

    def analyze(self):
        """Analyze results and generate reports."""
        if self.metrics:
            self.metrics.generate_report()
            self.metrics.save_results()


class Scenario1_Baseline(Scenario):
    """Scenario 1: Baseline Performance"""

    def __init__(self, num_nodes=15, duration=300, message_interval=60):
        super().__init__(
            "Scenario_1_Baseline",
            "Stable network with low, steady traffic"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.message_interval = message_interval

    def setup(self):
        """Setup baseline scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        self.network.create_topology('random', connection_probability=0.3)
        self.metrics = MetricsCollector(self.name)

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'message_interval': self.message_interval,
            'topology': 'random'
        }

        print(f"Created network with {self.num_nodes} nodes")
        print(f"Root node: {self.network.root_node.node_id}")

    def run(self):
        """Run baseline scenario."""
        print(f"\nRunning simulation for {self.duration}s...")

        # Schedule periodic broadcasts from each node
        def periodic_broadcast(env, network, node_id, interval):
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        # Start periodic broadcasts for all nodes
        for node_id in self.network.nodes.keys():
            self.network.env.process(
                periodic_broadcast(self.network.env, self.network, node_id, self.message_interval)
            )

        # Run simulation
        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()

        # Collect metrics
        self.metrics.collect(self.network, self.params)


class Scenario2_HighTraffic(Scenario):
    """Scenario 2: High Traffic / Broadcast Storm"""

    def __init__(self, num_nodes=15, duration=60, message_interval=3):
        super().__init__(
            "Scenario_2_HighTraffic",
            "All nodes broadcasting at high frequency"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.message_interval = message_interval

    def setup(self):
        """Setup high traffic scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        self.network.create_topology('random', connection_probability=0.3)
        self.metrics = MetricsCollector(self.name)

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'message_interval': self.message_interval,
            'topology': 'random',
            'traffic_type': 'broadcast_storm'
        }

        print(f"Created network with {self.num_nodes} nodes")

    def run(self):
        """Run high traffic scenario."""
        print(f"\nRunning broadcast storm simulation...")

        # Schedule aggressive broadcasts
        def aggressive_broadcast(env, network, node_id, interval):
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        for node_id in self.network.nodes.keys():
            self.network.env.process(
                aggressive_broadcast(self.network.env, self.network, node_id, self.message_interval)
            )

        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()
        self.metrics.collect(self.network, self.params)


class Scenario3_RootFailure(Scenario):
    """Scenario 3: Root Node Failure"""

    def __init__(self, num_nodes=15, duration=120, failure_time=30):
        super().__init__(
            "Scenario_3_RootFailure",
            "Root node fails during operation"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.failure_time = failure_time

    def setup(self):
        """Setup root failure scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        self.network.create_topology('random', connection_probability=0.3)
        self.metrics = MetricsCollector(self.name)

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'failure_time': self.failure_time,
            'topology': 'random'
        }

        print(f"Created network with {self.num_nodes} nodes")
        print(f"Root node {self.network.root_node.node_id} will fail at t={self.failure_time}s")

    def run(self):
        """Run root failure scenario."""
        print(f"\nRunning root failure simulation...")

        root_id = self.network.root_node.node_id

        # Schedule normal traffic
        def periodic_broadcast(env, network, node_id, interval):
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        # Schedule root failure
        def root_failure(env, network, fail_time, node_id):
            yield env.timeout(fail_time)
            print(f"\n[t={env.now:.2f}s] ROOT NODE {node_id} FAILED!")
            network.remove_node(node_id)

        # Start traffic
        for node_id in self.network.nodes.keys():
            self.network.env.process(
                periodic_broadcast(self.network.env, self.network, node_id, 20)
            )

        # Schedule failure
        self.network.env.process(
            root_failure(self.network.env, self.network, self.failure_time, root_id)
        )

        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()
        self.metrics.collect(self.network, self.params)


class Scenario4_IntermediateFailure(Scenario):
    """Scenario 4: Intermediate Node Failure"""

    def __init__(self, num_nodes=20, duration=120, failure_time=30):
        super().__init__(
            "Scenario_4_IntermediateFailure",
            "Bridge node fails, orphaning children"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.failure_time = failure_time

    def setup(self):
        """Setup intermediate node failure scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        self.network.create_topology('random', connection_probability=0.25)
        self.metrics = MetricsCollector(self.name)

        # Find a node with multiple children (a bridge node)
        self.bridge_node = None
        for node in self.network.nodes.values():
            if not node.is_root and len(node.children) >= 2:
                self.bridge_node = node
                break

        if not self.bridge_node:
            # If no suitable bridge, pick any non-root node
            for node in self.network.nodes.values():
                if not node.is_root:
                    self.bridge_node = node
                    break

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'failure_time': self.failure_time,
            'failed_node': self.bridge_node.node_id if self.bridge_node else None,
            'topology': 'random'
        }

        print(f"Bridge node {self.bridge_node.node_id} will fail at t={self.failure_time}s")

    def run(self):
        """Run intermediate node failure scenario."""
        print(f"\nRunning intermediate node failure simulation...")

        def periodic_broadcast(env, network, node_id, interval):
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        def node_failure(env, network, fail_time, node_id):
            yield env.timeout(fail_time)
            print(f"\n[t={env.now:.2f}s] NODE {node_id} FAILED!")
            network.remove_node(node_id)

        for node_id in self.network.nodes.keys():
            self.network.env.process(
                periodic_broadcast(self.network.env, self.network, node_id, 20)
            )

        if self.bridge_node:
            self.network.env.process(
                node_failure(self.network.env, self.network,
                           self.failure_time, self.bridge_node.node_id)
            )

        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()
        self.metrics.collect(self.network, self.params)


class Scenario5_Scalability(Scenario):
    """Scenario 5: Network Scalability"""

    def __init__(self, start_nodes=5, max_nodes=50, step=5, duration_per_size=60):
        super().__init__(
            "Scenario_5_Scalability",
            "Test network performance as size increases"
        )
        self.start_nodes = start_nodes
        self.max_nodes = max_nodes
        self.step = step
        self.duration_per_size = duration_per_size
        self.results = []

    def setup(self):
        """Setup scalability scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")
        print(f"Will test from {self.start_nodes} to {self.max_nodes} nodes (step={self.step})")

        self.metrics = MetricsCollector(self.name)
        self.params = {
            'start_nodes': self.start_nodes,
            'max_nodes': self.max_nodes,
            'step': self.step,
            'duration_per_size': self.duration_per_size
        }

    def run(self):
        """Run scalability tests."""
        print(f"\nRunning scalability tests...")

        for num_nodes in range(self.start_nodes, self.max_nodes + 1, self.step):
            print(f"\n--- Testing with {num_nodes} nodes ---")

            # Create network
            network = Network(num_nodes=num_nodes)
            network.create_topology('random', connection_probability=0.3)

            # Run simulation
            def periodic_broadcast(env, network, node_id, interval):
                while True:
                    yield env.timeout(interval)
                    if node_id in network.nodes and network.nodes[node_id].active:
                        network.send_message(node_id, None, 100, 'broadcast')

            for node_id in network.nodes.keys():
                network.env.process(
                    periodic_broadcast(network.env, network, node_id, 30)
                )

            network.run(self.duration_per_size)

            # Collect stats
            stats = network.get_statistics()
            self.results.append({
                'num_nodes': num_nodes,
                'stats': stats
            })

            print(f"Nodes: {num_nodes}, Loss: {stats['packet_loss_rate']:.2f}%, "
                  f"Avg Latency: {stats['latency']['avg']*1000:.2f}ms" if stats['latency'] else "N/A")

        # Store final results
        self.params['results'] = self.results
        if self.results:
            self.metrics.results = {
                'scenario': self.name,
                'parameters': self.params,
                'statistics': self.results[-1]['stats']  # Last run stats
            }

        print("\nScalability test complete!")


class Scenario6_Bottleneck(Scenario):
    """Scenario 6: Single-Node Bottleneck"""

    def __init__(self, num_nodes=15, duration=60, target_node_id=None):
        super().__init__(
            "Scenario_6_Bottleneck",
            "All nodes send to single target node"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.target_node_id = target_node_id

    def setup(self):
        """Setup bottleneck scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        self.network.create_topology('random', connection_probability=0.3)
        self.metrics = MetricsCollector(self.name)

        # Choose target node (not root)
        if self.target_node_id is None:
            for node in self.network.nodes.values():
                if not node.is_root:
                    self.target_node_id = node.node_id
                    break

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'target_node_id': self.target_node_id,
            'topology': 'random'
        }

        print(f"All nodes will send to target node {self.target_node_id}")

    def run(self):
        """Run bottleneck scenario."""
        print(f"\nRunning bottleneck simulation...")

        def aggressive_unicast(env, network, source_id, target_id, interval):
            while True:
                if (source_id in network.nodes and network.nodes[source_id].active and
                    source_id != target_id):
                    network.send_message(source_id, target_id, 100, 'unicast')
                yield env.timeout(interval)

        for node_id in self.network.nodes.keys():
            self.network.env.process(
                aggressive_unicast(self.network.env, self.network,
                                 node_id, self.target_node_id, 2)
            )

        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()

        # Show bottleneck node stats
        if self.target_node_id in self.network.nodes:
            target = self.network.nodes[self.target_node_id]
            print(f"\nBottleneck Node {self.target_node_id} Stats:")
            print(f"  Received: {target.messages_received}")
            print(f"  Max Queue: {max(target.queue_length_samples) if target.queue_length_samples else 0}")

        self.metrics.collect(self.network, self.params)


class Scenario7_NetworkPartition(Scenario):
    """Scenario 7: Network Partition (Split-Brain)"""

    def __init__(self, num_nodes=20, duration=120, partition_time=30):
        super().__init__(
            "Scenario_7_NetworkPartition",
            "Network splits into disconnected sub-meshes"
        )
        self.num_nodes = num_nodes
        self.duration = duration
        self.partition_time = partition_time

    def setup(self):
        """Setup partition scenario."""
        print(f"\n{'='*60}")
        print(f"Setting up {self.name}")
        print(f"{'='*60}")

        self.network = Network(num_nodes=self.num_nodes)
        # Create a topology that can be easily partitioned
        self.network.create_topology('chain')
        self.metrics = MetricsCollector(self.name)

        # Find middle node(s) to remove
        middle = len(self.network.nodes) // 2
        self.partition_nodes = [middle, middle + 1]

        self.params = {
            'num_nodes': self.num_nodes,
            'duration': self.duration,
            'partition_time': self.partition_time,
            'partition_nodes': self.partition_nodes,
            'topology': 'chain'
        }

        print(f"Will partition network by removing nodes {self.partition_nodes} at t={self.partition_time}s")

    def run(self):
        """Run partition scenario."""
        print(f"\nRunning network partition simulation...")

        def periodic_broadcast(env, network, node_id, interval):
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        def create_partition(env, network, partition_time, node_ids):
            yield env.timeout(partition_time)
            print(f"\n[t={env.now:.2f}s] NETWORK PARTITION! Removing nodes {node_ids}")
            for node_id in node_ids:
                if node_id in network.nodes:
                    network.remove_node(node_id)

        for node_id in self.network.nodes.keys():
            self.network.env.process(
                periodic_broadcast(self.network.env, self.network, node_id, 20)
            )

        self.network.env.process(
            create_partition(self.network.env, self.network,
                           self.partition_time, self.partition_nodes)
        )

        self.network.run(self.duration)

        print("Simulation complete!")
        self.network.print_summary()
        self.metrics.collect(self.network, self.params)
