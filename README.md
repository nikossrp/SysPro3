# SysPro3
CyclicBuffer is a simple shared structure that will store files for the threads. Each thread gets a file from cyclicbuffer and inserts entries into the vaccineMonitor/project1 dataset.
I'm using sockets for the communication between processes, everything else is the same as the SysPro2, .

<p><h3>Compile</h3></p>
make

<h3><p>Usage</h3></p>
<p>./travelMonitorClient –m numMonitors -b socketBufferSize -c cyclicBufferSize -s sizeOfBloom -i input_dir -t numThreads </p>

<!-- <h3><p>Description</h3></p>
cyclicBuffer: is a simple shared structure that will store files for the threads.
Everything else is the same as the SysPro2 but for communication between processes using sockets instead of using named pipes. -->
