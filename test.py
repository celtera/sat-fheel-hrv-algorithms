import pyheartbeat_metrics
import random
random.seed(42)

obj = pyheartbeat_metrics.heartbeat_metrics()
obj.recording = True
for i in range(50):
    for j in range(20):
        obj.input(f"{i}", random.randrange(35+i, 55+i))

obj.recording = False


obj.Excitation = lambda x: print(x)
obj.Synchronization = lambda x: print(x)
obj.stddev_range = 1.2
spread = 15
base = 65
high = 30
low = 5
for i in range(50):
    basei = base+i
    if random.random() < 0.05:
        obj.input(f"{i}", random.randrange(basei-spread+high, basei+spread+high))
        print("high")
    elif random.random() < 0.05:
        obj.input(f"{i}", random.randrange(basei-spread-low, basei+spread-low))
        print("low")
    else:
        obj.input(f"{i}", random.randrange(35+i, 55+i))
