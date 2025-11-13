"""
Main entry point for mesh network simulation.
Run scenarios and generate reports.
"""

import argparse
import sys
from scenarios import (
    Scenario1_Baseline,
    Scenario2_HighTraffic,
    Scenario3_RootFailure,
    Scenario4_IntermediateFailure,
    Scenario5_Scalability,
    Scenario6_Bottleneck,
    Scenario7_NetworkPartition
)


def run_scenario(scenario_class, **kwargs):
    """Run a single scenario."""
    try:
        scenario = scenario_class(**kwargs)
        scenario.setup()
        scenario.run()
        scenario.analyze()

        # Generate visualizations if network available
        if hasattr(scenario, 'network') and scenario.network:
            output_prefix = f"results/{scenario.name}"
            scenario.metrics.plot_latency_distribution(
                scenario.network,
                f"{output_prefix}_latency.png"
            )
            scenario.metrics.plot_node_load(
                scenario.network,
                f"{output_prefix}_load.png"
            )

        print(f"\n[OK] {scenario.name} completed successfully!")
        return True

    except Exception as e:
        print(f"\n[FAILED] {scenario.name} failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def run_all_scenarios():
    """Run all scenarios sequentially."""
    print("\n" + "="*60)
    print("MESH NETWORK SIMULATION - RUNNING ALL SCENARIOS")
    print("="*60)

    scenarios = [
        (Scenario1_Baseline, {}),
        (Scenario2_HighTraffic, {}),
        (Scenario3_RootFailure, {}),
        (Scenario4_IntermediateFailure, {}),
        (Scenario5_Scalability, {'max_nodes': 30, 'step': 5}),
        (Scenario6_Bottleneck, {}),
        (Scenario7_NetworkPartition, {})
    ]

    results = []
    for scenario_class, kwargs in scenarios:
        success = run_scenario(scenario_class, **kwargs)
        results.append((scenario_class.__name__, success))

    # Print summary
    print("\n" + "="*60)
    print("SIMULATION SUMMARY")
    print("="*60)
    for name, success in results:
        status = "[PASS]" if success else "[FAIL]"
        print(f"{name:40} {status}")
    print("="*60)

    passed = sum(1 for _, s in results if s)
    total = len(results)
    print(f"\nTotal: {passed}/{total} scenarios passed")

    return all(s for _, s in results)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Mesh Network Simulation for EspHub',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python main.py --all                    Run all scenarios
  python main.py --scenario 1             Run scenario 1 (Baseline)
  python main.py --scenario 3             Run scenario 3 (Root Failure)
  python main.py --scenario 5 --nodes 50  Run scenario 5 with 50 max nodes
        """
    )

    parser.add_argument(
        '--all',
        action='store_true',
        help='Run all scenarios'
    )

    parser.add_argument(
        '--scenario',
        type=int,
        choices=range(1, 8),
        help='Run a specific scenario (1-7)'
    )

    parser.add_argument(
        '--nodes',
        type=int,
        default=15,
        help='Number of nodes (default: 15)'
    )

    parser.add_argument(
        '--duration',
        type=int,
        default=None,
        help='Simulation duration in seconds'
    )

    args = parser.parse_args()

    if args.all:
        success = run_all_scenarios()
        sys.exit(0 if success else 1)

    elif args.scenario:
        scenario_map = {
            1: Scenario1_Baseline,
            2: Scenario2_HighTraffic,
            3: Scenario3_RootFailure,
            4: Scenario4_IntermediateFailure,
            5: Scenario5_Scalability,
            6: Scenario6_Bottleneck,
            7: Scenario7_NetworkPartition
        }

        scenario_class = scenario_map[args.scenario]
        kwargs = {'num_nodes': args.nodes}

        if args.duration:
            kwargs['duration'] = args.duration

        # Special handling for scenario 5
        if args.scenario == 5:
            kwargs = {'max_nodes': args.nodes, 'step': 5}

        success = run_scenario(scenario_class, **kwargs)
        sys.exit(0 if success else 1)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
