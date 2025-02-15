import os
import xml.etree.ElementTree as ET
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Define time intervals for mobility and protocol changes
NO_MOBILITY = (0, 24)
WITH_MOBILITY = (24, 48)
PROTOCOL_CHANGE_INTERVAL = 8  # 8-second intervals for protocol changes

def parse_flow_monitor(file_path):
    """Parses XML file and extracts flow statistics, delay histogram, and jitter histogram."""
    print(f"Processing: {file_path}")
    tree = ET.parse(file_path)
    root = tree.getroot()

    flow_stats = []
    delay_hist = []
    jitter_hist = []

    for flow_statistics in root.findall(".//FlowStats"):
        for flow in flow_statistics.findall("Flow"):
            try:
                time_first_tx = float(flow.get("timeFirstTxPacket").replace("ns", "")) * 1e-9  # Convert ns to s
                flow_info = {
                    "flowId": int(flow.get("flowId")),
                    "timeFirstTx": time_first_tx,
                    "delaySum": float(flow.get("delaySum").replace("ns", "")),
                    "jitterSum": float(flow.get("jitterSum").replace("ns", "")),
                    "txPackets": int(flow.get("txPackets")),
                    "rxPackets": int(flow.get("rxPackets")),
                    "lostPackets": int(flow.get("lostPackets")),
                }
                flow_stats.append(flow_info)

                for bin_elem in flow.findall(".//delayHistogram/bin"):
                    delay_hist.append((float(bin_elem.get("start")), int(bin_elem.get("count"))))

                for bin_elem in flow.findall(".//jitterHistogram/bin"):
                    jitter_hist.append((float(bin_elem.get("start")), int(bin_elem.get("count"))))

            except Exception as e:
                print(f"Error processing flow {flow.get('flowId')}: {e}")
                
    return flow_stats, delay_hist, jitter_hist

def plot_histogram(data, title, xlabel, ylabel, filename, norm=False):
    """Plots a histogram with optional normalization."""
    if not data:
        return

    bins, counts = zip(*data)
    
    if norm:
        counts = np.array(counts) / max(counts)  # Normalize values

    plt.figure(figsize=(10, 5))
    plt.bar(bins, counts, width=max(bins) * 0.02, edgecolor='black', alpha=0.7)
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.savefig(filename)
    plt.close()

def compare_flows(flows, filename, title):
    """Compares delay, jitter, and loss between different flows."""
    if not flows:
        return

    flow_ids = [f["flowId"] for f in flows]
    delays = np.array([f["delaySum"] / f["rxPackets"] for f in flows if f["rxPackets"] > 0])
    jitters = np.array([f["jitterSum"] / f["rxPackets"] for f in flows if f["rxPackets"] > 0])
    loss_rates = np.array([f["lostPackets"] / f["txPackets"] if f["txPackets"] > 0 else 0 for f in flows])

    plt.figure(figsize=(12, 6))
    plt.plot(flow_ids, delays, label="Delay", marker="o")
    plt.plot(flow_ids, jitters, label="Jitter", marker="s")
    plt.plot(flow_ids, loss_rates, label="Packet Loss", marker="^")

    plt.xlabel("Flow ID")
    plt.ylabel("Value")
    plt.title(title)
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.savefig(filename)
    plt.show()

def plot_boxplot(data, title, ylabel, filename):
    """Plots a boxplot for statistical analysis."""
    plt.figure(figsize=(8, 6))
    plt.boxplot(data, vert=True, patch_artist=True)
    plt.title(title)
    plt.ylabel(ylabel)
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.savefig(filename)
    plt.close()

def scatter_plot(x, y, xlabel, ylabel, title, filename):
    """Plots a scatter plot to show correlation between parameters."""
    plt.figure(figsize=(8, 6))
    plt.scatter(x, y, alpha=0.7)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.savefig(filename)
    plt.close()

def separate_by_experiment_phase(all_flow_stats):
    """Separate flows by experimental phase: No mobility, With mobility, and protocol phases."""
    no_mobility_flows = [fs for fs in all_flow_stats if NO_MOBILITY[0] <= fs["timeFirstTx"] < NO_MOBILITY[1]]
    with_mobility_flows = [fs for fs in all_flow_stats if WITH_MOBILITY[0] <= fs["timeFirstTx"] < WITH_MOBILITY[1]]

    protocol_phase_flows = {
        "udp_no_mobility": [fs for fs in all_flow_stats if 1 <= fs["flowId"] <= 8],
        "tcp_no_mobility": [fs for fs in all_flow_stats if 9 <= fs["flowId"] <= 16],
        "hybrid_no_mobility": [fs for fs in all_flow_stats if 17 <= fs["flowId"] <= 24],
        "udp_with_mobility": [fs for fs in all_flow_stats if 25 <= fs["flowId"] <= 32],
        "tcp_with_mobility": [fs for fs in all_flow_stats if 33 <= fs["flowId"] <= 40],
        "hybrid_with_mobility": [fs for fs in all_flow_stats if 41 <= fs["flowId"] <= 48],
    }

    return no_mobility_flows, with_mobility_flows, protocol_phase_flows

def calculate_statistics(flows):
    """Calculate and return statistical metrics for the given flows."""
    delays = [f["delaySum"] / f["rxPackets"] for f in flows if f["rxPackets"] > 0]
    jitters = [f["jitterSum"] / f["rxPackets"] for f in flows if f["rxPackets"] > 0]
    loss_rates = [f["lostPackets"] / f["txPackets"] if f["txPackets"] > 0 else 0 for f in flows]

    stats = {
        "delays": {
            "mean": np.mean(delays),
            "median": np.median(delays),
            "std": np.std(delays),
            "cv": np.std(delays) / np.mean(delays) * 100,  # Coefficient of variation
            "percentiles": np.percentile(delays, [25, 50, 75])
        },
        "jitters": {
            "mean": np.mean(jitters),
            "median": np.median(jitters),
            "std": np.std(jitters),
            "cv": np.std(jitters) / np.mean(jitters) * 100,  # Coefficient of variation
            "percentiles": np.percentile(jitters, [25, 50, 75])
        },
        "loss_rates": {
            "mean": np.mean(loss_rates),
            "median": np.median(loss_rates),
            "std": np.std(loss_rates),
            "cv": np.std(loss_rates) / np.mean(loss_rates) * 100,  # Coefficient of variation
            "percentiles": np.percentile(loss_rates, [25, 50, 75])
        }
    }

    return stats

def process_files(directory):
    """Processes all XML files in a directory and generates analysis plots."""
    all_flow_stats = []
    all_delay_hist = []
    all_jitter_hist = []

    for filename in os.listdir(directory):
        if filename.startswith("resultados_") and filename.endswith(".xml"):
            file_path = os.path.join(directory, filename)
            flow_stats, delay_hist, jitter_hist = parse_flow_monitor(file_path)
            all_flow_stats.extend(flow_stats)
            all_delay_hist.extend(delay_hist)
            all_jitter_hist.extend(jitter_hist)

    # Separate data by experiment phases
    no_mobility_flows, with_mobility_flows, protocol_phase_flows = separate_by_experiment_phase(all_flow_stats)

    # Plot histograms
    plot_histogram(all_delay_hist, "Delay Histogram", "Delay (ns)", "Count", "delay_histogram.png", norm=True)
    plot_histogram(all_jitter_hist, "Jitter Histogram", "Jitter (ns)", "Count", "jitter_histogram.png", norm=True)

    # Compare flows for different mobility and protocol phases
    compare_flows(no_mobility_flows, "flow_no_mobility.png", "Flows (No Mobility)")
    compare_flows(with_mobility_flows, "flow_with_mobility.png", "Flows (With Mobility)")

    # Boxplot for delay distribution
    delays = [fs["delaySum"] / fs["rxPackets"] for fs in all_flow_stats if fs["rxPackets"] > 0]
    plot_boxplot(delays, "Delay Distribution", "Delay (ns)", "boxplot_delay.png")

    # Scatter plot: packet loss vs delay
    loss_rates = [fs["lostPackets"] / fs["txPackets"] if fs["txPackets"] > 0 else 0 for fs in all_flow_stats]
    scatter_plot(delays, loss_rates, "Delay (ns)", "Packet Loss (%)", "Delay vs Packet Loss", "scatter_delay_loss.png")

    # Plot comparison for each protocol phase with statistical analysis
    for phase, flows in protocol_phase_flows.items():
        compare_flows(flows, f"flow_{phase}.png", f"Flows ({phase})")
        stats = calculate_statistics(flows)
        print(f"Statistics for {phase}: {stats}")

if __name__ == "__main__":
    process_files(".")
