"""
Message class for mesh network simulation.
Represents a data packet in the network.
"""

class Message:
    """Represents a message/packet in the mesh network."""

    _next_id = 0

    def __init__(self, source_id, destination_id, payload_size, creation_time, msg_type='unicast'):
        """
        Initialize a message.

        Args:
            source_id: ID of the node that created this message
            destination_id: ID of the destination node (None for broadcast)
            payload_size: Size of the payload in bytes
            creation_time: Simulation time when message was created
            msg_type: Type of message ('unicast', 'broadcast', 'control')
        """
        self.id = Message._next_id
        Message._next_id += 1

        self.source_id = source_id
        self.destination_id = destination_id
        self.payload_size = payload_size
        self.creation_time = creation_time
        self.msg_type = msg_type

        # Routing information
        self.current_node = source_id
        self.hop_count = 0
        self.path = [source_id]  # Track the path the message takes

        # Metrics
        self.delivery_time = None
        self.total_latency = None
        self.delivered = False
        self.lost = False

    def record_hop(self, node_id, current_time):
        """Record that the message passed through a node."""
        self.hop_count += 1
        self.current_node = node_id
        self.path.append(node_id)

    def mark_delivered(self, delivery_time):
        """Mark the message as successfully delivered."""
        self.delivered = True
        self.delivery_time = delivery_time
        self.total_latency = delivery_time - self.creation_time

    def mark_lost(self):
        """Mark the message as lost."""
        self.lost = True

    def is_broadcast(self):
        """Check if this is a broadcast message."""
        return self.msg_type == 'broadcast' or self.destination_id is None

    def __repr__(self):
        return (f"Message(id={self.id}, src={self.source_id}, "
                f"dst={self.destination_id}, type={self.msg_type}, "
                f"hops={self.hop_count})")
