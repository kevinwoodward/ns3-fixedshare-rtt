# Flowmon parser. For retreving delivery ratio, retransmission ratio, and goodput from a .flowmon file.

from xml.etree import ElementTree as ET

pathstr = "/Users/kevin/Downloads/ns-allinone-3.30.1/ns-3.30.1/s4.flowmon"
tree = ET.parse(pathstr)
root = tree.getroot()
print(root)

# Gather all of the TCP flows only
flowIds = []
for child in root[1]:
    if (child.attrib['protocol'] is not '6'):
        continue
    if (int(child[0].attrib['packets']) > 1):
        flowIds.append(child.attrib['flowId'])
    
flows = []
# Get the flowstats for each TCP flow
for child in root[0]:
    if (child.attrib['flowId'] in flowIds and child.attrib['timeFirstRxPacket'] != child.attrib['timeLastRxPacket'] and child.attrib['timeFirstTxPacket'] != child.attrib['timeLastTxPacket']):
            flows.append(child)

goodputSum = 0
deliveryRatioSum = 0
retransmitSum = 0
txPacketSum = 0

# Get the goodput and delivery ratio out of each and apply a weighted average
for flow in flows:
    rxPackets = float(flow.attrib['rxPackets'])
    txPackets = float(flow.attrib['txPackets'])

    # Convert from ns to seconds
    firstRx = float(flow.attrib['timeFirstRxPacket'][1:-4]) / 1e+9 
    lastRx = float(flow.attrib['timeLastRxPacket'][1:-4]) / 1e+9
    
    rxTime = lastRx - firstRx
    
    # Unit: packets/second
    rxRate = rxPackets / rxTime
    
    txPacketSum += txPackets
    
    retransmitSum += (txPackets - rxPackets) # / txPackets (multiplying by txPackets for weighted sum)
    
    deliveryRatioSum += (rxPackets) # /txPackets) (multiplying by txPackets for weighted sum)

    goodputSum += rxRate * txPackets # (multiplying by txPackets for weighted sum)
    
# Unweighted average. Not logically accurate.
# print('Goodput (p/s): ', goodputSum/len(flows))
# print('Delivery ratio(%): ', deliveryRatioSum/len(flows))
# print('Retransmit ratio(%): ', retransmitSum/len(flows))

# Weighted average of each where weight is based on transmitted packets
print('Packet weight: ', txPacketSum)
print('Goodput (p/s): ', goodputSum/txPacketSum)
print('Delivery ratio(%): ', deliveryRatioSum/txPacketSum)
print('Retransmit ratio(%): ', retransmitSum/txPacketSum)











