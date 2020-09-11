# PinPoint

A tool for energy profiling.
At the moment the following platforms are supported:

* 3-channel INA3221 on NVIDIA Jetson TX2 boards
* 3-channel INA3221 on NVIDIA Jetson AGX Xavier boards
* Microchip MCP39F511N (for external power measurements)

The interface is to some extent inspired by `perf stat`.

	Usage: pinpoint -h|[-c|-p] [-e dev1,dev2,...] ([-r|-d|-i|-b|-a] N)* [--] workload [args]
		-h Print this help and exit
		-l Print a list of available counters and exit
		-c Continuously print power levels (mW) to stdout (skip energy stats)
		-p Measure energy-delayed product (measure joules if not provided)
		-e Comma seperated list of measured devices (default: all)
		-r Number of runs (default: 1)
		-d Delay between runs in ms (default: 0)
		-i Sampling interval in ms (default: 500)
		-b Start measurement N ms before worker creation (negative values will delay start)
		-a Continue measurement N ms after worker exited

Use this tool if you want to start/stop your measurements with your program, or test your implementation of a new data source for power measurements.

While this user-space tool has no third-party dependencies, future work might move its functionality and power sources to [libpapi](https://icl.utk.edu/papi/) or create virtual [`perf`](https://perf.wiki.kernel.org) events to provide better integration with other tools.

### Examples

#### Basic Example

Sample 4 runs of `heatmap` workload in 4 runs with 250ms interval. Mean, along with ratio of std-dev/mean is printed:

	$ pinpoint -r 4 -i 250 -- ./heatmap 1000 1000 1000 random.csv
	Tegra energy counter stats for './heatmap 1000 1000 1000 random.csv':
	[interval: 250ms, before: 0 ms, after: 0 ms, delay: 0 ms, runs: 4]

		5609.31 mJ	CPU	( +- 5.37% )
		2311.00 mJ	GPU	( +- 0.03% )
		2404.06 mJ	SOC	( +- 1.06% )
		4496.69 mJ	DDR	( +- 1.20% )
		18372.19 mJ	IN	( +- 3.05% )

		2.87523996 seconds time elapsed ( +- 1.09% )

Note, the `--` seperating arguments from workload call can be skipped, if the workload has no arguments that might be mistaken by POSIX `getopt` as arguments to `pinpoint`. If you want to play safe, include the seperator.

#### Selecting Counters, Trimming Your Time Range

Sometimes your workload might have a known warm-up phase, or keeps the system in a specific energy-state after execution.
In case you want to account for these effects, `pinpoint` allows you to.

Roughly same call as above, but this time only CPU and GPU are sampled. Additionally, the measurement is started 100ms after child process creation and kept running for 3s. Between last sample and start of a new run, there is a 5s delay.

	$ pinpoint -e CPU,GPU -r 4 -i 250 -b -100 -a 3000 -d 5000 -- ./heatmap 1000 1000 1000 random.csv
	Tegra energy counter stats for './heatmap 1000 1000 1000 random.csv':
	[interval: 250ms, before: -100 ms, after: 3000 ms, delay: 5000 ms, runs: 4]

		6206.56 mJ	CPU	( +- 0.52% )
		4629.25 mJ	GPU	( +- 0.01% )

		2.85707129 seconds time elapsed ( +- 0.02% )

#### Energy-Delayed Product

The Energy-Delayed Product (EDP) is another metric, that takes into account that one can trade computation time for energy consumption. The EDP is defined as product of a job's energy demand and its execution time.

You could compute the EDP manually from `pinpoint`'s output, or let it compute automatically by passing `-p`. This is usefull with multiple runs, where otherwise you are only provided with mean values.

	$ pinpoint -p -e CPU,GPU -r 4 -i 250 -- ./heatmap 1000 1000 1000 random.csv
	Tegra energy counter stats for './heatmap 1000 1000 1000 random.csv':
	[interval: 250ms, before: 0 ms, after: 0 ms, delay: 0 ms, runs: 4]

		15756.88 mJs	CPU	( +- 5.26% )
		6597.88 mJs	GPU	( +- 0.12% )

		2.85491275 seconds time elapsed ( +- 0.10% )

#### List Raw Names of Available Data Sources

When called with `-l`, `pinpoint` will list all accessible data sources on the current system. Those sources are identified by a `:`-seperated tuple of the source class name and the raw counter name. As they might differ between different platforms, there also exists a list of aliases mapping human-friendly names to raw counter names.

On a Jetson TX2, with directly attached MCP39F511N this might look like this:

	$ pinpoint -l
	List of available counters (to be used in -e):
	  jetson:VDD_4V0_WIFI
	  jetson:VDD_IN
	  jetson:VDD_SYS_CPU
	  jetson:VDD_SYS_DDR
	  jetson:VDD_SYS_GPU
	  jetson:VDD_SYS_SOC
	  mcp:dev0ch1
	  mcp:dev0ch2

	List of available aliases:
	  CPU -> jetson:VDD_SYS_CPU
	  DDR -> jetson:VDD_SYS_DDR
	  EXT_IN -> mcp:dev0ch1
	  GPU -> jetson:VDD_SYS_GPU
	  IN -> jetson:VDD_IN
	  MCP1 -> mcp:dev0ch1
	  MCP2 -> mcp:dev0ch2
	  MEM -> jetson:VDD_SYS_DDR
	  SOC -> jetson:VDD_SYS_SOC
	  WIFI -> jetson:VDD_4V0_WIFI

#### Continuously Print Power Levels

You can mimic the behavior of our older `osmtegrastats` by passing `-c`, just with the added benefits of automatic start/stop of measurements with your workload and timed trimming and delay features. If multiple runs are specified, a seperator line will be included.

	$ pinpoint -c -r 2 -e CPU,DDR -- ./heatmap 1000 1000 500 random.csv
	### Run 0
	193,1108
	1445,1394
	1878,1490
	1879,1413
	### Run 1
	723,1203
	1590,1413
	1878,1509
	1926,1413

