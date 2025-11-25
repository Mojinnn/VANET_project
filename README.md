# Simulate emergency accident alert system via VANET using SUMO and NS3

This repository contains the final-term project for the **Network Programming course**, focusing on simulating and analyzing **Vehicular Ad-Hoc Networks (VANETs)**. The project integrates **Network Simulator 2 (NS-2)**, **SUMO (Simulation of Urban Mobility)**, mobility trace generation, and packet-level network analysis to evaluate VANET communication behavior.

---

## 🎯 Project Objectives

* Build a realistic vehicular mobility scenario using **SUMO**.
* Integrate SUMO output into **NS-2** through a mobility TCL script.
* Simulate wireless communication between vehicles based on VANET standards.
* Capture network traffic and analyze packet delivery patterns.
* Document network performance through a written report and presentation.

---

## 📁 Repository Structure

```
VANET_project/
│
├── Src/                # NS-2 simulation scripts and network configuration code
│
├── Docs/               # Final report & presentation slides
│   ├── report.pdf
│   └── presentation.pptx
│
├── SUMO/               # SUMO scenario files for mobility generation
│   ├── network files
│   ├── route configurations
│   └── output used to create mobility.tcl
│
├── Result/             # Simulation output and analysis data
│   ├── *.pcap          # Packet captures for Wireshark analysis
│   ├── *.xml           # Output for visualization or data processing
│   └── *.tcl           # SUMO visual/metrics result scripts
│
└── README.md           # Project introduction (this file)
```

---

## 🛠️ Technologies & Tools Used

| Component                 | Tool/Protocol      |
| ------------------------- | ------------------ |
| Network simulation        | NS-2               |
| Vehicular mobility        | SUMO               |
| Packet capture & analysis | PCAP, Wireshark    |
| Scripting                 | TCL                |
| Documentation             | LaTeX / PowerPoint |
| Data representation       | XML                |

---

## 🚗 Mobility Generation Workflow

1. Design road network & vehicle flows in SUMO.
2. Export vehicle mobility trace.
3. Convert SUMO output into **mobility.tcl**.
4. Import mobility.tcl into NS-2 simulation.

This ensures realistic vehicle movements for VANET simulation.

---

## 📡 Network Simulation Overview

* Wireless ad-hoc routing
* Vehicle-to-Vehicle (V2V) communication
* Event-driven simulation via NS-2
* Performance observation through packet capture & trace logs

---

## 📊 Result & Analysis

Simulation outputs are stored in the `Result/` directory:

* **PCAP files** — For analyzing packet transmission, latency, packet loss
* **XML files** — For structured performance visualization and statistics
* **TCL files** — For visual replay or extended SUMO result evaluation

Tools like **Wireshark**, **SUMO-GUI**, or **NS-2 NAM** may be used for deeper analysis.

---

## ▶️ How to Run the Simulation

1. Ensure SUMO and NS-2 are installed.
2. Generate or modify mobility files using SUMO (optional).
3. Navigate to the `Src/` directory.
4. Run the NS-2 simulation script:

```bash
ns main.tcl
```

5. Open results from the `Result/` folder for analysis.

---

## 📚 Documentation

See `Docs/` for:

* Full written report
* Presentation slide deck
* Methodology, architecture, evaluation, and conclusion

---

## 👨‍💻 Author

**Mojinnn** — Computer Engineering student

This project was developed as part of the **Network Programming final-term coursework**.

---

## ✅ Status

✔️ Completed & Presented

---

## 💡 Future Improvements

* Implement IEEE 802.11p / DSRC or C-V2X
* Add congestion & traffic control models
* Multi-hop routing performance comparison
* Visualization dashboards

---

## 📜 License

This project is intended for academic and research purposes.

