"""
Stress test scenario for large-scale mesh network.
500 nodes, max depth 10 hops, 200 active senders at 2 msg/sec
"""

import simpy
from network import Network
from metrics import MetricsCollector
import random


class StressTestScenario:
    """Scenario: Large-scale stress test with controlled topology depth"""

    def __init__(self,
                 total_nodes=500,
                 active_senders=200,
                 max_depth=10,
                 messages_per_second=2,
                 duration=60):
        """
        Initialize stress test scenario.

        Args:
            total_nodes: Total number of nodes in network (default 500)
            active_senders: Number of nodes actively sending messages (default 200)
            max_depth: Maximum tree depth in hops (default 10)
            messages_per_second: Messages per second per active node (default 2)
            duration: Simulation duration in seconds (default 60)
        """
        self.name = f"StressTest_{total_nodes}nodes_{active_senders}active"
        self.description = f"{total_nodes} nodes, max {max_depth} hops, {active_senders} sending {messages_per_second} msg/s"

        self.total_nodes = total_nodes
        self.active_senders = active_senders
        self.max_depth = max_depth
        self.messages_per_second = messages_per_second
        self.message_interval = 1.0 / messages_per_second  # Convert to interval
        self.duration = duration

        self.network = None
        self.metrics = None
        self.params = {}
        self.active_sender_ids = []

    def setup(self):
        """Setup the stress test scenario."""
        print(f"\n{'='*70}")
        print(f"STRESS TEST SETUP: {self.description}")
        print(f"{'='*70}")
        print(f"Creating network with {self.total_nodes} nodes...")

        # Create network
        self.network = Network(num_nodes=self.total_nodes)

        # Create balanced tree topology with depth constraint
        print(f"Building tree topology (max depth: {self.max_depth} hops)...")
        self._create_depth_limited_tree()

        # Select random active senders
        all_node_ids = list(self.network.nodes.keys())
        self.active_sender_ids = random.sample(all_node_ids,
                                              min(self.active_senders, len(all_node_ids)))

        # Initialize metrics
        self.metrics = MetricsCollector(self.name)

        self.params = {
            'total_nodes': self.total_nodes,
            'active_senders': self.active_senders,
            'max_depth': self.max_depth,
            'messages_per_second': self.messages_per_second,
            'message_interval': self.message_interval,
            'duration': self.duration,
            'topology': 'balanced_tree_depth_limited'
        }

        print(f"\n[Setup Complete]")
        print(f"  Total Nodes: {len(self.network.nodes)}")
        print(f"  Root Node: {self.network.root_node.node_id}")
        print(f"  Active Senders: {len(self.active_sender_ids)}")
        print(f"  Message Rate: {self.messages_per_second} msg/s per sender")
        print(f"  Total Message Rate: {self.messages_per_second * len(self.active_sender_ids)} msg/s")
        print(f"  Tree Max Depth: {self.max_depth} hops")
        print(f"  Simulation Duration: {self.duration}s")

        # Calculate tree statistics
        self._print_tree_statistics()

    def _create_depth_limited_tree(self):
        """Create a balanced tree with depth limitation."""
        # Calculate children per node to stay within depth limit
        # For N nodes and max depth D: children_per_node ≈ (N)^(1/D)
        children_per_node = int((self.total_nodes) ** (1.0 / self.max_depth))
        children_per_node = max(2, min(children_per_node, 10))  # Keep between 2-10

        print(f"  Using ~{children_per_node} children per node")

        # Set root
        root_id = 1
        self.network.root_node = self.network.nodes[root_id]
        self.network.root_node.is_root = True
        self.network.root_node.set_parent(None)

        # Build tree level by level
        current_level = [self.network.root_node]
        remaining_nodes = [n for n in self.network.nodes.values() if n.node_id != root_id]
        current_depth = 0

        while remaining_nodes and current_depth < self.max_depth:
            next_level = []

            for parent_node in current_level:
                # Assign children to this parent
                num_children = min(children_per_node, len(remaining_nodes))

                for _ in range(num_children):
                    if not remaining_nodes:
                        break

                    child = remaining_nodes.pop(0)
                    child.set_parent(parent_node)

                    # Add as neighbor for connectivity
                    parent_node.add_neighbor(child)

                    next_level.append(child)

            current_level = next_level
            current_depth += 1

        # If there are remaining nodes (shouldn't happen with proper calculation),
        # attach them to random leaf nodes
        if remaining_nodes:
            print(f"  Warning: {len(remaining_nodes)} nodes couldn't fit in depth limit")
            leaf_nodes = [n for n in self.network.nodes.values() if len(n.children) == 0 and not n.is_root]
            for node in remaining_nodes:
                if leaf_nodes:
                    parent = random.choice(leaf_nodes)
                    node.set_parent(parent)
                    parent.add_neighbor(node)

    def _print_tree_statistics(self):
        """Print tree topology statistics."""
        # Calculate actual tree depth
        def get_depth(node, current_depth=0):
            if not node.children:
                return current_depth
            return max(get_depth(child, current_depth + 1) for child in node.children)

        actual_depth = get_depth(self.network.root_node)

        # Calculate node distribution by depth
        depth_distribution = {}
        def count_by_depth(node, depth=0):
            depth_distribution[depth] = depth_distribution.get(depth, 0) + 1
            for child in node.children:
                count_by_depth(child, depth + 1)

        count_by_depth(self.network.root_node)

        # Calculate children per node statistics
        children_counts = [len(n.children) for n in self.network.nodes.values() if not n.is_root]
        avg_children = sum(children_counts) / len(children_counts) if children_counts else 0
        max_children = max(children_counts) if children_counts else 0

        print(f"\n[Tree Statistics]")
        print(f"  Actual Tree Depth: {actual_depth} hops")
        print(f"  Average Children per Node: {avg_children:.2f}")
        print(f"  Max Children per Node: {max_children}")
        print(f"  Root Node Children: {len(self.network.root_node.children)}")
        print(f"\n  Node Distribution by Depth:")
        for depth in sorted(depth_distribution.keys()):
            count = depth_distribution[depth]
            percentage = (count / self.total_nodes) * 100
            bar = '#' * int(percentage / 2)
            print(f"    Depth {depth:2d}: {count:4d} nodes ({percentage:5.1f}%) {bar}")

    def run(self):
        """Run the stress test scenario."""
        print(f"\n{'='*70}")
        print(f"RUNNING STRESS TEST")
        print(f"{'='*70}")
        print(f"Expected total messages: ~{int(self.duration * self.messages_per_second * len(self.active_sender_ids))}")
        print(f"Simulation time: {self.duration}s")
        print(f"\nStarting simulation...")

        # Create message sending processes for active nodes
        def active_sender(env, network, node_id, interval):
            """Process for nodes that actively send messages."""
            while True:
                if node_id in network.nodes and network.nodes[node_id].active:
                    # Send broadcast message
                    network.send_message(node_id, None, 100, 'broadcast')
                yield env.timeout(interval)

        # Start sender processes
        for node_id in self.active_sender_ids:
            self.network.env.process(
                active_sender(self.network.env, self.network, node_id, self.message_interval)
            )

        # Run simulation
        start_time = self.network.env.now
        self.network.run(self.duration)

        print(f"\n{'='*70}")
        print(f"SIMULATION COMPLETE")
        print(f"{'='*70}")

        # Print summary
        self.network.print_summary()

        # Collect detailed statistics
        self._print_detailed_statistics()

        # Collect metrics
        self.metrics.collect(self.network, self.params)

    def _print_detailed_statistics(self):
        """Print detailed network load statistics."""
        print(f"\n{'='*70}")
        print(f"DETAILED LOAD STATISTICS")
        print(f"{'='*70}")

        stats = self.network.get_statistics()
        node_stats = stats['node_statistics']

        # Sort nodes by forwarded messages (load indicator)
        sorted_nodes = sorted(node_stats, key=lambda x: x['messages_forwarded'], reverse=True)

        # Top 10 most loaded nodes
        print(f"\nTop 10 Most Loaded Nodes:")
        print(f"{'Node':>6} {'Root':>6} {'Depth':>6} {'Children':>8} {'Sent':>8} {'Recv':>8} {'Fwd':>8} {'Dropped':>8} {'MaxQ':>6}")
        print(f"{'-'*70}")

        for i, node_stat in enumerate(sorted_nodes[:10]):
            node_id = node_stat['node_id']
            node = self.network.nodes[node_id]

            # Calculate depth
            depth = 0
            current = node
            while current.parent:
                depth += 1
                current = current.parent

            print(f"{node_id:6d} {'Yes' if node_stat['is_root'] else 'No':>6} {depth:6d} "
                  f"{node_stat['num_children']:8d} {node_stat['messages_sent']:8d} "
                  f"{node_stat['messages_received']:8d} {node_stat['messages_forwarded']:8d} "
                  f"{node_stat['messages_dropped']:8d} {node_stat['max_queue_length']:6d}")

        # Calculate load by depth
        print(f"\nLoad Distribution by Tree Depth:")
        print(f"{'Depth':>6} {'Nodes':>6} {'Avg Fwd':>10} {'Avg Recv':>10} {'Avg Dropped':>12}")
        print(f"{'-'*50}")

        depth_stats = {}
        for node_stat in node_stats:
            node_id = node_stat['node_id']
            node = self.network.nodes[node_id]

            # Calculate depth
            depth = 0
            current = node
            while current.parent:
                depth += 1
                current = current.parent

            if depth not in depth_stats:
                depth_stats[depth] = {
                    'count': 0,
                    'total_fwd': 0,
                    'total_recv': 0,
                    'total_dropped': 0
                }

            depth_stats[depth]['count'] += 1
            depth_stats[depth]['total_fwd'] += node_stat['messages_forwarded']
            depth_stats[depth]['total_recv'] += node_stat['messages_received']
            depth_stats[depth]['total_dropped'] += node_stat['messages_dropped']

        for depth in sorted(depth_stats.keys()):
            ds = depth_stats[depth]
            avg_fwd = ds['total_fwd'] / ds['count']
            avg_recv = ds['total_recv'] / ds['count']
            avg_dropped = ds['total_dropped'] / ds['count']

            print(f"{depth:6d} {ds['count']:6d} {avg_fwd:10.1f} {avg_recv:10.1f} {avg_dropped:12.1f}")

        # Overall statistics
        total_sent = sum(n['messages_sent'] for n in node_stats)
        total_received = sum(n['messages_received'] for n in node_stats)
        total_forwarded = sum(n['messages_forwarded'] for n in node_stats)
        total_dropped = sum(n['messages_dropped'] for n in node_stats)

        print(f"\nOverall Message Statistics:")
        print(f"  Total Sent: {total_sent}")
        print(f"  Total Received: {total_received}")
        print(f"  Total Forwarded: {total_forwarded}")
        print(f"  Total Dropped: {total_dropped}")
        print(f"  Effective Throughput: {stats['delivered_messages']} deliveries")

        if stats['latency']:
            print(f"\nLatency Statistics:")
            print(f"  Average: {stats['latency']['avg']*1000:.2f} ms")
            print(f"  Minimum: {stats['latency']['min']*1000:.2f} ms")
            print(f"  Maximum: {stats['latency']['max']*1000:.2f} ms")
            print(f"  P95: {stats['latency']['p95']*1000:.2f} ms")

    def analyze(self):
        """Analyze results and generate reports."""
        if self.metrics:
            print(f"\nGenerating reports...")
            self.metrics.generate_report()
            self.metrics.save_results()

            # Generate visualizations
            if self.network:
                output_prefix = f"results/{self.name}"
                try:
                    self.metrics.plot_latency_distribution(
                        self.network,
                        f"{output_prefix}_latency.png"
                    )
                    self.metrics.plot_node_load(
                        self.network,
                        f"{output_prefix}_load.png"
                    )
                    print(f"Visualizations saved to results/")
                except Exception as e:
                    print(f"Note: Visualization generation skipped ({e})")


def main():
    """Run the stress test."""
    import argparse

    parser = argparse.ArgumentParser(description='Large-scale mesh network stress test')
    parser.add_argument('--nodes', type=int, default=500, help='Total number of nodes (default: 500)')
    parser.add_argument('--active', type=int, default=200, help='Number of active senders (default: 200)')
    parser.add_argument('--depth', type=int, default=10, help='Max tree depth in hops (default: 10)')
    parser.add_argument('--rate', type=int, default=2, help='Messages per second per node (default: 2)')
    parser.add_argument('--duration', type=int, default=60, help='Simulation duration in seconds (default: 60)')

    args = parser.parse_args()

    print(f"\n{'='*70}")
    print(f"MESH NETWORK STRESS TEST")
    print(f"{'='*70}")
    print(f"Configuration:")
    print(f"  Total Nodes: {args.nodes}")
    print(f"  Active Senders: {args.active}")
    print(f"  Max Tree Depth: {args.depth} hops")
    print(f"  Message Rate: {args.rate} msg/s per sender")
    print(f"  Total Network Load: {args.rate * args.active} msg/s")
    print(f"  Duration: {args.duration}s")
    print(f"  Expected Messages: ~{args.rate * args.active * args.duration}")
    print(f"{'='*70}\n")

    # Create and run scenario
    scenario = StressTestScenario(
        total_nodes=args.nodes,
        active_senders=args.active,
        max_depth=args.depth,
        messages_per_second=args.rate,
        duration=args.duration
    )

    scenario.setup()
    scenario.run()
    scenario.analyze()

    print(f"\n{'='*70}")
    print(f"STRESS TEST COMPLETE")
    print(f"{'='*70}")
    print(f"Results saved to results/ directory")
    print(f"Review the detailed report and visualizations for analysis")


if __name__ == '__main__':
    main()
