"""
Advanced visualization tools for mesh network simulation results.
"""

import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import json
from pathlib import Path
import numpy as np


class SimulationVisualizer:
    """Create visualizations from simulation results."""

    def __init__(self, results_dir='results'):
        self.results_dir = Path(results_dir)
        sns.set_style("whitegrid")
        plt.rcParams['figure.figsize'] = (12, 6)

    def load_results(self, scenario_name):
        """Load results JSON file for a scenario."""
        pattern = f"{scenario_name}_*.json"
        files = list(self.results_dir.glob(pattern))

        if not files:
            print(f"No results found for {scenario_name}")
            return None

        # Get most recent file
        latest = max(files, key=lambda p: p.stat().st_mtime)
        print(f"Loading: {latest}")

        with open(latest, 'r') as f:
            return json.load(f)

    def plot_scenario_comparison(self, output_file='comparison.png'):
        """Compare metrics across all scenarios."""
        scenarios = [
            'Scenario_1_Baseline',
            'Scenario_2_HighTraffic',
            'Scenario_3_RootFailure',
            'Scenario_4_IntermediateFailure',
            'Scenario_6_Bottleneck'
        ]

        data = []
        for scenario in scenarios:
            results = self.load_results(scenario)
            if results and results['statistics'].get('latency'):
                data.append({
                    'Scenario': scenario.replace('Scenario_', 'S').replace('_', ' '),
                    'Avg Latency (ms)': results['statistics']['latency']['avg'] * 1000,
                    'Packet Loss (%)': results['statistics']['packet_loss_rate'],
                    'Total Messages': results['statistics']['total_messages']
                })

        if not data:
            print("No data available for comparison")
            return

        df = pd.DataFrame(data)

        # Create subplots
        fig, axes = plt.subplots(1, 2, figsize=(14, 5))

        # Latency comparison
        axes[0].bar(df['Scenario'], df['Avg Latency (ms)'], color='steelblue')
        axes[0].set_xlabel('Scenario')
        axes[0].set_ylabel('Average Latency (ms)')
        axes[0].set_title('Latency Comparison Across Scenarios')
        axes[0].tick_params(axis='x', rotation=45)
        axes[0].grid(True, alpha=0.3)

        # Packet loss comparison
        axes[1].bar(df['Scenario'], df['Packet Loss (%)'], color='coral')
        axes[1].set_xlabel('Scenario')
        axes[1].set_ylabel('Packet Loss (%)')
        axes[1].set_title('Packet Loss Comparison Across Scenarios')
        axes[1].tick_params(axis='x', rotation=45)
        axes[1].grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        print(f"Comparison plot saved to: {output_file}")

    def plot_scalability_results(self, output_file='scalability.png'):
        """Plot scalability test results."""
        results = self.load_results('Scenario_5_Scalability')

        if not results or 'results' not in results['parameters']:
            print("No scalability results found")
            return

        data = results['parameters']['results']

        nodes = [r['num_nodes'] for r in data]
        latencies = [r['stats']['latency']['avg'] * 1000 if r['stats']['latency'] else 0
                    for r in data]
        packet_loss = [r['stats']['packet_loss_rate'] for r in data]

        fig, axes = plt.subplots(1, 2, figsize=(14, 5))

        # Latency vs nodes
        axes[0].plot(nodes, latencies, marker='o', linewidth=2, markersize=8, color='steelblue')
        axes[0].set_xlabel('Number of Nodes')
        axes[0].set_ylabel('Average Latency (ms)')
        axes[0].set_title('Network Scalability - Latency')
        axes[0].grid(True, alpha=0.3)

        # Packet loss vs nodes
        axes[1].plot(nodes, packet_loss, marker='s', linewidth=2, markersize=8, color='coral')
        axes[1].set_xlabel('Number of Nodes')
        axes[1].set_ylabel('Packet Loss Rate (%)')
        axes[1].set_title('Network Scalability - Packet Loss')
        axes[1].grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        print(f"Scalability plot saved to: {output_file}")

    def plot_node_statistics_heatmap(self, scenario_name, output_file=None):
        """Create a heatmap of node statistics."""
        results = self.load_results(scenario_name)

        if not results:
            return

        node_stats = results['statistics']['node_statistics']

        # Create DataFrame
        df = pd.DataFrame(node_stats)
        df = df.set_index('node_id')

        # Select numeric columns
        numeric_cols = ['messages_sent', 'messages_received', 'messages_forwarded',
                       'messages_dropped', 'max_queue_length']
        df_numeric = df[numeric_cols]

        # Normalize for better visualization
        df_normalized = (df_numeric - df_numeric.min()) / (df_numeric.max() - df_numeric.min())

        # Create heatmap
        plt.figure(figsize=(10, 8))
        sns.heatmap(df_normalized.T, annot=df_numeric.T, fmt='g',
                   cmap='YlOrRd', cbar_kws={'label': 'Normalized Value'})
        plt.title(f'{scenario_name} - Node Activity Heatmap')
        plt.xlabel('Node ID')
        plt.ylabel('Metric')
        plt.tight_layout()

        if output_file is None:
            output_file = f"results/{scenario_name}_heatmap.png"

        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        print(f"Heatmap saved to: {output_file}")

    def generate_all_visualizations(self):
        """Generate all available visualizations."""
        print("\nGenerating visualizations...")

        # Comparison plot
        self.plot_scenario_comparison('results/all_scenarios_comparison.png')

        # Scalability plot
        self.plot_scalability_results('results/scalability_analysis.png')

        # Heatmaps for each scenario
        scenarios = [
            'Scenario_1_Baseline',
            'Scenario_2_HighTraffic',
            'Scenario_3_RootFailure'
        ]

        for scenario in scenarios:
            self.plot_node_statistics_heatmap(scenario)

        print("\n[OK] All visualizations generated!")


def main():
    """Main entry point for visualization script."""
    import argparse

    parser = argparse.ArgumentParser(description='Generate visualizations from simulation results')
    parser.add_argument('--all', action='store_true', help='Generate all visualizations')
    parser.add_argument('--compare', action='store_true', help='Generate comparison plot')
    parser.add_argument('--scalability', action='store_true', help='Generate scalability plot')
    parser.add_argument('--heatmap', type=str, help='Generate heatmap for specific scenario')

    args = parser.parse_args()

    viz = SimulationVisualizer()

    if args.all:
        viz.generate_all_visualizations()
    elif args.compare:
        viz.plot_scenario_comparison()
    elif args.scalability:
        viz.plot_scalability_results()
    elif args.heatmap:
        viz.plot_node_statistics_heatmap(args.heatmap)
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
