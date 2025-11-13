"""
Metrics collection and analysis for mesh network simulation.
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import json
from pathlib import Path


class MetricsCollector:
    """Collects and analyzes simulation metrics."""

    def __init__(self, scenario_name):
        """
        Initialize the metrics collector.

        Args:
            scenario_name: Name of the scenario being tested
        """
        self.scenario_name = scenario_name
        self.results = {}

    def collect(self, network, scenario_params):
        """
        Collect metrics from a completed simulation.

        Args:
            network: The Network instance after simulation
            scenario_params: Dictionary of scenario parameters
        """
        stats = network.get_statistics()

        self.results = {
            'scenario': self.scenario_name,
            'parameters': scenario_params,
            'timestamp': pd.Timestamp.now().isoformat(),
            'statistics': stats
        }

        return self.results

    def save_results(self, output_dir='results'):
        """Save results to JSON file."""
        Path(output_dir).mkdir(exist_ok=True)

        filename = f"{output_dir}/{self.scenario_name}_{pd.Timestamp.now().strftime('%Y%m%d_%H%M%S')}.json"

        with open(filename, 'w') as f:
            json.dump(self.results, f, indent=2, default=str)

        print(f"Results saved to: {filename}")
        return filename

    def generate_report(self, output_dir='results'):
        """Generate a markdown report of the results."""
        if not self.results:
            print("No results to report")
            return

        Path(output_dir).mkdir(exist_ok=True)
        filename = f"{output_dir}/{self.scenario_name}_report.md"

        stats = self.results['statistics']

        with open(filename, 'w', encoding='utf-8') as f:
            f.write(f"# {self.scenario_name} - Simulation Report\n\n")
            f.write(f"**Generated:** {self.results['timestamp']}\n\n")

            # Scenario parameters
            f.write("## Scenario Parameters\n\n")
            for key, value in self.results['parameters'].items():
                f.write(f"- **{key}:** {value}\n")

            # Network overview
            f.write("\n## Network Overview\n\n")
            f.write(f"- **Total Nodes:** {stats['total_nodes']}\n")
            f.write(f"- **Active Nodes:** {stats['active_nodes']}\n")
            f.write(f"- **Root Node ID:** {stats['root_node_id']}\n")

            # Message statistics
            f.write("\n## Message Statistics\n\n")
            f.write(f"- **Total Messages:** {stats['total_messages']}\n")
            f.write(f"- **Delivered:** {stats['delivered_messages']}\n")
            f.write(f"- **Lost:** {stats['lost_messages']}\n")
            f.write(f"- **Packet Loss Rate:** {stats['packet_loss_rate']:.2f}%\n")

            # Latency statistics
            if stats['latency']:
                f.write("\n## Latency Statistics\n\n")
                lat = stats['latency']
                f.write(f"- **Average:** {lat['avg']*1000:.2f} ms\n")
                f.write(f"- **Minimum:** {lat['min']*1000:.2f} ms\n")
                f.write(f"- **Maximum:** {lat['max']*1000:.2f} ms\n")
                f.write(f"- **Median:** {lat['median']*1000:.2f} ms\n")
                f.write(f"- **Std Dev:** {lat['stdev']*1000:.2f} ms\n")
                f.write(f"- **95th Percentile:** {lat['p95']*1000:.2f} ms\n")

            # Node statistics
            f.write("\n## Node Statistics\n\n")
            f.write("| Node ID | Is Root | Sent | Received | Forwarded | Dropped | Avg Queue | Max Queue |\n")
            f.write("|---------|---------|------|----------|-----------|---------|-----------|----------|\n")

            for node_stat in stats['node_statistics']:
                f.write(f"| {node_stat['node_id']} | "
                       f"{'Yes' if node_stat['is_root'] else 'No'} | "
                       f"{node_stat['messages_sent']} | "
                       f"{node_stat['messages_received']} | "
                       f"{node_stat['messages_forwarded']} | "
                       f"{node_stat['messages_dropped']} | "
                       f"{node_stat['avg_queue_length']:.2f} | "
                       f"{node_stat['max_queue_length']} |\n")

            # Events timeline
            if stats['events']:
                f.write("\n## Events Timeline\n\n")
                f.write("| Time | Event Type | Node ID | Details |\n")
                f.write("|------|------------|---------|----------|\n")

                for event in stats['events']:
                    details = ', '.join([f"{k}={v}" for k, v in event.items()
                                       if k not in ['time', 'type', 'node_id']])
                    f.write(f"| {event['time']:.3f}s | "
                           f"{event['type']} | "
                           f"{event.get('node_id', 'N/A')} | "
                           f"{details} |\n")

            # Analysis and observations
            f.write("\n## Analysis\n\n")
            f.write(self._generate_analysis(stats))

        print(f"Report saved to: {filename}")
        return filename

    def _generate_analysis(self, stats):
        """Generate analysis text based on statistics."""
        analysis = []

        # Packet loss analysis
        if stats['packet_loss_rate'] > 10:
            analysis.append(f"- **High packet loss detected ({stats['packet_loss_rate']:.2f}%)**: "
                          "This indicates network congestion or instability. Consider reducing traffic "
                          "or increasing node processing capacity.")
        elif stats['packet_loss_rate'] > 1:
            analysis.append(f"- **Moderate packet loss ({stats['packet_loss_rate']:.2f}%)**: "
                          "Some messages are being dropped. This is acceptable for some applications "
                          "but may indicate approaching capacity limits.")
        else:
            analysis.append(f"- **Low packet loss ({stats['packet_loss_rate']:.2f}%)**: "
                          "Network is operating within acceptable parameters.")

        # Latency analysis
        if stats['latency']:
            avg_latency_ms = stats['latency']['avg'] * 1000
            if avg_latency_ms > 1000:
                analysis.append(f"- **High average latency ({avg_latency_ms:.2f}ms)**: "
                              "Messages are experiencing significant delays. This may not be suitable "
                              "for real-time applications.")
            elif avg_latency_ms > 100:
                analysis.append(f"- **Moderate latency ({avg_latency_ms:.2f}ms)**: "
                              "Acceptable for most applications but may be noticeable in interactive scenarios.")
            else:
                analysis.append(f"- **Low latency ({avg_latency_ms:.2f}ms)**: "
                              "Excellent response time for mesh network.")

        # Node load analysis
        root_nodes = [n for n in stats['node_statistics'] if n['is_root']]
        if root_nodes:
            root = root_nodes[0]
            if root['messages_forwarded'] > stats['total_messages'] * 0.5:
                analysis.append(f"- **Root node is heavily loaded**: "
                              f"Node {root['node_id']} forwarded {root['messages_forwarded']} messages. "
                              "Consider load balancing or network restructuring.")

        # Queue analysis
        max_queues = [n['max_queue_length'] for n in stats['node_statistics']]
        if max_queues and max(max_queues) > 50:
            analysis.append(f"- **High queue lengths detected (max: {max(max_queues)})**: "
                          "Nodes are struggling to process messages quickly enough. "
                          "This can lead to increased latency and packet loss.")

        if not analysis:
            analysis.append("- No significant issues detected. Network is operating normally.")

        return '\n'.join(analysis)

    def plot_latency_distribution(self, network, output_file='latency_dist.png'):
        """Plot latency distribution."""
        latencies = [m.total_latency * 1000 for m in network.all_messages
                    if m.delivered and m.total_latency is not None]

        if not latencies:
            print("No latency data to plot")
            return

        plt.figure(figsize=(10, 6))
        plt.hist(latencies, bins=30, edgecolor='black', alpha=0.7)
        plt.xlabel('Latency (ms)')
        plt.ylabel('Frequency')
        plt.title(f'{self.scenario_name} - Latency Distribution')
        plt.grid(True, alpha=0.3)
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        print(f"Latency plot saved to: {output_file}")

    def plot_node_load(self, network, output_file='node_load.png'):
        """Plot node load comparison."""
        stats = network.get_statistics()
        node_stats = stats['node_statistics']

        node_ids = [n['node_id'] for n in node_stats]
        forwarded = [n['messages_forwarded'] for n in node_stats]

        plt.figure(figsize=(12, 6))
        bars = plt.bar(node_ids, forwarded)

        # Highlight root node
        for i, n in enumerate(node_stats):
            if n['is_root']:
                bars[i].set_color('red')

        plt.xlabel('Node ID')
        plt.ylabel('Messages Forwarded')
        plt.title(f'{self.scenario_name} - Node Load (Messages Forwarded)')
        plt.grid(True, alpha=0.3, axis='y')
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        print(f"Node load plot saved to: {output_file}")
