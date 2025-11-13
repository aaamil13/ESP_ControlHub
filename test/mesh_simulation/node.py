"""
Node class for mesh network simulation.
Represents a single ESP32 device in the painlessMesh network.
"""

import simpy
from collections import deque
from message import Message


class Node:
    """Represents a single node in the mesh network."""

    def __init__(self, env, node_id, network=None, processing_delay=0.005):
        """
        Initialize a node.

        Args:
            env: SimPy environment
            node_id: Unique identifier for this node
            network: Reference to the Network object
            processing_delay: Time in seconds to process each message (default 5ms)
        """
        self.env = env
        self.node_id = node_id
        self.network = network
        self.processing_delay = processing_delay

        # Network topology
        self.parent = None
        self.children = set()
        self.neighbors = set()  # Directly reachable nodes
        self.is_root = False

        # Message queues
        self.message_queue = deque()
        self.max_queue_size = 100  # Maximum queue size before dropping packets

        # Metrics
        self.messages_sent = 0
        self.messages_received = 0
        self.messages_forwarded = 0
        self.messages_dropped = 0
        self.queue_length_samples = []
        self.load_samples = []

        # State
        self.active = True
        self.last_parent_update = 0

        # Statistics for analysis
        self.total_processing_time = 0
        self.message_delays = []

        # Start the node's main process
        self.process = env.process(self.run())

    def run(self):
        """Main process loop for the node."""
        while self.active:
            # Sample metrics periodically
            if len(self.message_queue) > 0:
                yield self.env.timeout(0)  # Process immediately if messages waiting
                message = self.message_queue.popleft()
                yield self.env.process(self.process_message(message))
            else:
                # Record queue length when idle
                self.record_metrics()
                yield self.env.timeout(0.1)  # Check again in 100ms

    def process_message(self, message):
        """Process a received message."""
        start_time = self.env.now

        # Simulate processing delay
        yield self.env.timeout(self.processing_delay)

        self.messages_received += 1
        message.record_hop(self.node_id, self.env.now)

        # Check if this message is for us
        if message.destination_id == self.node_id:
            # Unicast message delivered
            message.mark_delivered(self.env.now)
            if self.network:
                self.network.register_delivery()
        elif message.is_broadcast():
            # Broadcast message - mark as delivered (each copy is for everyone)
            message.mark_delivered(self.env.now)
            if self.network:
                self.network.register_delivery()
            # Continue forwarding to other nodes
        else:
            # Need to forward this message
            self.messages_forwarded += 1

        # Forward message to appropriate next hop(s)
        if message.is_broadcast():
            # Broadcast to parent and children, except the one it came from
            import copy
            from_node = message.path[-2] if len(message.path) > 1 else None

            recipients = []
            if self.parent and self.parent.node_id != from_node:
                recipients.append(self.parent)

            for child in list(self.children):
                if child.node_id != from_node:
                    recipients.append(child)

            for recipient in recipients:
                msg_copy = copy.deepcopy(message)
                recipient.receive_message(msg_copy)
        else:
            # Unicast routing
            next_hop = self.get_next_hop(message.destination_id)
            if next_hop:
                next_hop.receive_message(message)
            else:
                # No route found, message is lost
                message.mark_lost()
                self.messages_dropped += 1

        processing_time = self.env.now - start_time
        self.total_processing_time += processing_time
        self.message_delays.append(processing_time)

    def receive_message(self, message):
        """Receive a message from another node."""
        if len(self.message_queue) >= self.max_queue_size:
            # Queue is full, drop the message
            self.messages_dropped += 1
            message.mark_lost()
            return False

        self.message_queue.append(message)
        return True

    def send_message(self, destination_id, payload_size, msg_type='unicast'):
        """Create and send a new message."""
        message = Message(
            source_id=self.node_id,
            destination_id=destination_id,
            payload_size=payload_size,
            creation_time=self.env.now,
            msg_type=msg_type
        )

        self.messages_sent += 1

        # Start routing the message
        if msg_type == 'broadcast':
            # In a tree topology, broadcast goes to parent and all children
            recipients = []
            if self.parent:
                recipients.append(self.parent)
            recipients.extend(list(self.children))

            for recipient in recipients:
                import copy
                msg_copy = copy.deepcopy(message)
                recipient.receive_message(msg_copy)
        else:
            # Route towards destination
            next_hop = self.get_next_hop(destination_id)
            if next_hop:
                next_hop.receive_message(message)
            else:
                message.mark_lost()
                self.messages_dropped += 1

        return message

    def get_next_hop(self, destination_id):
        """
        Determine the next hop for a message to reach destination.
        In a tree topology, messages go up to parent or down to children.
        """
        # Simple routing: if we're not the root, send to parent
        # In a real implementation, this would check if destination is in our subtree
        if self.is_root:
            # We're root, check children
            for child in self.children:
                if self._is_in_subtree(destination_id, child):
                    return child
            return None
        else:
            # Not root, send to parent (it will know how to route)
            return self.parent

    def _is_in_subtree(self, node_id, subtree_root):
        """Check if a node_id is in the subtree rooted at subtree_root."""
        # Simplified: would need network-wide knowledge
        # For simulation, we'll track this in the Network class
        if subtree_root.node_id == node_id:
            return True
        for child in subtree_root.children:
            if self._is_in_subtree(node_id, child):
                return True
        return False

    def set_parent(self, parent_node):
        """Set the parent node in the mesh tree."""
        if self.parent:
            self.parent.children.discard(self)

        self.parent = parent_node
        if parent_node:
            parent_node.children.add(self)
            self.is_root = False
        else:
            self.is_root = True

        self.last_parent_update = self.env.now

    def add_neighbor(self, neighbor_node):
        """Add a node as a neighbor (directly reachable)."""
        self.neighbors.add(neighbor_node)
        neighbor_node.neighbors.add(self)

    def remove_neighbor(self, neighbor_node):
        """Remove a neighbor."""
        self.neighbors.discard(neighbor_node)
        neighbor_node.neighbors.discard(self)

    def deactivate(self):
        """Deactivate this node (simulate failure)."""
        self.active = False
        # Notify children to find new parent
        for child in list(self.children):
            child.parent = None
            child.children.clear()

    def record_metrics(self):
        """Record current metrics for analysis."""
        self.queue_length_samples.append(len(self.message_queue))

        # Calculate load: weighted sum of operations
        load = (self.messages_sent * 1.0 +
                self.messages_received * 1.0 +
                self.messages_forwarded * 2.0)  # Forwarding is more expensive
        self.load_samples.append((self.env.now, load))

    def get_statistics(self):
        """Get statistics for this node."""
        avg_queue_length = (sum(self.queue_length_samples) / len(self.queue_length_samples)
                            if self.queue_length_samples else 0)
        max_queue_length = max(self.queue_length_samples) if self.queue_length_samples else 0

        return {
            'node_id': self.node_id,
            'is_root': self.is_root,
            'messages_sent': self.messages_sent,
            'messages_received': self.messages_received,
            'messages_forwarded': self.messages_forwarded,
            'messages_dropped': self.messages_dropped,
            'avg_queue_length': avg_queue_length,
            'max_queue_length': max_queue_length,
            'total_processing_time': self.total_processing_time,
            'num_children': len(self.children)
        }

    def __repr__(self):
        return f"Node(id={self.node_id}, root={self.is_root}, children={len(self.children)})"
