# Cwnd file parser. For retreving mean cwnd size in bytes values for one run

pathstr = "/Users/kevin/Downloads/ns-allinone-3.30.1/ns-3.30.1/s3.cwnd"

fdata = []
with open (pathstr) as file:
    fdata = file.readlines()
    # 536 division to get units of packets. Packet length is (512 + header) = 536
    fdatanumeric = [float(i) for i in fdata]
    
    # No need to weight an average as input is raw data, not avg per flow
    print ('Weight: ', len(fdatanumeric))
    print ("Average cwnd size: ", sum(fdatanumeric)/len(fdatanumeric))
