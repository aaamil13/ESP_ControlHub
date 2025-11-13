"""
Network class for mesh network simulation.
Manages the overall simulation including topology, nodes, and metrics collection.
"""

import simpy
import networkx as nx
from node import Node
from message import Message
import statistics


class Network:
    """Manages the mesh network simulation."""

    def __init__(self, num_nodes=10):
        """
        Initialize the network simulation.

        Args:
            num_nodes: Initial number of nodes in the network
        """
        self.env = simpy.Environment()
        self.nodes = {}
        self.graph = nx.Graph()
        self.root_node = None
        self.num_nodes = num_nodes

        # Metrics
        self.all_messages = []
        self.delivered_count = 0  # Track delivered messages
        self.events = []  # Timeline of events

        # Create initial nodes
        self._create_nodes(num_nodes)

    def _create_nodes(self, count):
        """Create a specified number of nodes."""
        for i in range(count):
            node_id = len(self.nodes) + 1
            node = Node(self.env, node_id, network=self)
            self.nodes[node_id] = node
            self.graph.add_node(node_id)

    def add_node(self):
        """Add a new node to the network."""
        node_id = len(self.nodes) + 1
        node = Node(self.env, node_id, network=self)
        self.nodes[node_id] = node
        self.graph.add_node(node_id)
        return node

    def remove_node(self, node_id):
        """Remove a node from the network (simulate failure)."""
        if node_id in self.nodes:
            node = self.nodes[node_id]
            self.events.append({
                'time': self.env.now,
                'type': 'node_failure',
                'node_id': node_id,
                'was_root': node.is_root
            })

            # Deactivate the node
            node.deactivate()

            # Remove from graph
            self.graph.remove_node(node_id)

            # If this was the root, trigger network restructuring
            if node.is_root:
                self._elect_new_root()

    def create_topology(self, topology_type='random', **kwargs):
        """
        Create a network topology.

        Args:
            topology_type: Type of topology ('random', 'star', 'chain', 'custom')
            **kwargs: Additional parameters for topology creation
        """
        if topology_type == 'random':
            self._create_random_topology(**kwargs)
        elif topology_type == 'star':
            self._create_star_topology()
        elif topology_type == 'chain':
            self._create_chain_topology()
        elif topology_type == 'custom':
            # Allow custom connections
            pass

        # Build the mesh tree structure
        self._build_mesh_tree()

    def _create_random_topology(self, connection_probability=0.3, min_connections=2):
        """Create a random topology with specified connection probability."""
        node_ids = list(self.nodes.keys())

        # Ensure minimum connectivity
        for node_id in node_ids:
            node = self.nodes[node_id]

            # Connect to some random neighbors
            potential_neighbors = [n for n in node_ids if n != node_id]

            for neighbor_id in potential_neighbors:
                if neighbor_id not in [n.node_id for n in node.neighbors]:
                    import random
                    if random.random() < connection_probability:
                        neighbor = self.nodes[neighbor_id]
                        node.add_neighbor(neighbor)
                        self.graph.add_edge(node_id, neighbor_id)

        # Ensure all nodes have at least min_connections
        for node_id in node_ids:
            node = self.nodes[node_id]
            while len(node.neighbors) < min_connections:
                import random
                potential = [self.nodes[n] for n in node_ids
                           if n != node_id and n not in [nb.node_id for nb in node.neighbors]]
                if not potential:
                    break
                neighbor = random.choice(potential)
                node.add_neighbor(neighbor)
                self.graph.add_edge(node_id, neighbor.node_id)

    def _create_star_topology(self):
        """Create a star topology with node 1 as the center."""
        if len(self.nodes) < 2:
            return

        center_id = 1
        center = self.nodes[center_id]

        for node_id, node in self.nodes.items():
            if node_id != center_id:
                center.add_neighbor(node)
                self.graph.add_edge(center_id, node_id)

    def _create_chain_topology(self):
        """Create a chain topology."""
        node_ids = sorted(self.nodes.keys())

        for i in range(len(node_ids) - 1):
            node = self.nodes[node_ids[i]]
            next_node = self.nodes[node_ids[i + 1]]
            node.add_neighbor(next_node)
            self.graph.add_edge(node_ids[i], node_ids[i + 1])

    def _build_mesh_tree(self):
        """Build the mesh tree structure (painlessMesh uses a tree topology)."""
        if not self.nodes:
            return

        # Select root node (node with lowest ID or highest connectivity)
        root_id = min(self.nodes.keys())
        self.root_node = self.nodes[root_id]
        self.root_node.set_parent(None)  # Root has no parent
        self.root_node.is_root = True

        self.events.append({
            'time': self.env.now,
            'type': 'root_elected',
            'node_id': root_id
        })

        # Build tree using BFS from root
        visited = {root_id}
        queue = [self.root_node]

        while queue:
            current = queue.pop(0)

            for neighbor in current.neighbors:
                if neighbor.node_id not in visited:
                    visited.add(neighbor.node_id)
                    neighbor.set_parent(current)
                    queue.append(neighbor)

    def _elect_new_root(self):
        """Elect a new root node after root failure."""
        # Find node with most connections
        active_nodes = [n for n in self.nodes.values() if n.active]
        if not active_nodes:
            return

        new_root = max(active_nodes, key=lambda n: len(n.neighbors))
        new_root.set_parent(None)
        new_root.is_root = True
        self.root_node = new_root

        self.events.append({
            'time': self.env.now,
            'type': 'root_elected',
            'node_id': new_root.node_id
        })

        # Rebuild tree
        self._build_mesh_tree()

    def send_message(self, source_id, destination_id, payload_size, msg_type='unicast'):
        """Send a message from source to destination."""
        if source_id in self.nodes:
            message = self.nodes[source_id].send_message(destination_id, payload_size, msg_type)
            self.all_messages.append(message)
            return message
        return None

    def register_delivery(self):
        """Register a message delivery."""
        self.delivered_count += 1

    def broadcast_from_all_nodes(self, payload_size=100):
        """Have all nodes send a broadcast message."""
        for node_id in list(self.nodes.keys()):
            if self.nodes[node_id].active:
                self.send_message(node_id, None, payload_size, 'broadcast')

    def run(self, duration):
        """Run the simulation for a specified duration."""
        self.env.run(until=duration)

    def get_statistics(self):
        """Collect and return network-wide statistics."""
        # Collect per-node statistics
        node_stats = [node.get_statistics() for node in self.nodes.values()]

        # Calculate message statistics
        delivered_messages = [m for m in self.all_messages if m.delivered]
        lost_messages = [m for m in self.all_messages if m.lost]

        latencies = [m.total_latency for m in delivered_messages if m.total_latency is not None]

        stats = {
            'total_nodes': len(self.nodes),
            'active_nodes': sum(1 for n in self.nodes.values() if n.active),
            'root_node_id': self.root_node.node_id if self.root_node else None,
            'total_messages': len(self.all_messages),
            'delivered_messages': self.delivered_count,
            'lost_messages': len(lost_messages),
            'packet_loss_rate': len(lost_messages) / len(self.all_messages) * 100 if self.all_messages else 0,
            'node_statistics': node_stats,
            'events': self.events
        }

        if latencies:
            stats['latency'] = {
                'avg': statistics.mean(latencies),
                'min': min(latencies),
                'max': max(latencies),
                'median': statistics.median(latencies),
                'stdev': statistics.stdev(latencies) if len(latencies) > 1 else 0,
                'p95': self._percentile(latencies, 95)
            }
        else:
            stats['latency'] = None

        return stats

    def _percentile(self, data, percentile):
        """Calculate percentile of a dataset."""
        if not data:
            return None
        sorted_data = sorted(data)
        index = int(len(sorted_data) * percentile / 100)
        return sorted_data[min(index, len(sorted_data) - 1)]

    def print_summary(self):
        """Print a summary of the simulation results."""
        stats = self.get_statistics()

        print("\n" + "="*60)
        print("MESH NETWORK SIMULATION SUMMARY")
        print("="*60)
        print(f"Total Nodes: {stats['total_nodes']}")
        print(f"Active Nodes: {stats['active_nodes']}")
        print(f"Root Node: {stats['root_node_id']}")
        print(f"\nTotal Messages: {stats['total_messages']}")
        print(f"Delivered: {stats['delivered_messages']}")
        print(f"Lost: {stats['lost_messages']}")
        print(f"Packet Loss Rate: {stats['packet_loss_rate']:.2f}%")

        if stats['latency']:
            print(f"\nLatency Statistics (seconds):")
            print(f"  Average: {stats['latency']['avg']:.4f}s")
            print(f"  Min: {stats['latency']['min']:.4f}s")
            print(f"  Max: {stats['latency']['max']:.4f}s")
            print(f"  Median: {stats['latency']['median']:.4f}s")
            print(f"  Std Dev: {stats['latency']['stdev']:.4f}s")
            print(f"  P95: {stats['latency']['p95']:.4f}s")

        print("\n" + "="*60)
