# Fixed Shares RTT Estimation in ns-3 
Includes:

- Implementation of a custom RTT estimator as a modification to the ns-3 source code as proposed by [Nunes et. al.]([https://users.soe.ucsc.edu/~slukin/rtt_paper.pdf](https://users.soe.ucsc.edu/~slukin/rtt_paper.pdf))
- Implementation of several wireless ad-hoc scenarios to test efficacy of the estimator when using AODV routing to transmit FTP data. Varying scenario parameters include density of node/area, transmission rate, transmission size, etc.
- Python scripts for analysis and data visualization
### Prerequisites

- The [ns-3 simulator]([https://www.nsnam.org/](https://www.nsnam.org/))
- Python 3+, matplotlib for analyzing and visualizing results

## Fixed Shares RTT Estimator

![Fixed Share Diagram](https://github.com/kevinwoodward/ns3-fixedshare-rtt/blob/master/img/FixedSharesDiagram.png?raw=true)

Algorithms/equations for each component and results can be found in the paper under ./paper