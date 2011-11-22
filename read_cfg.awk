#!/usr/bin/awk
 	/enable_off_mode/{ print $2 > "/sys/power/enable_off_mode" }
	/sleep_while_idle/{ print $2 > "/sys/power/sleep_while_idle" }
	/sr_vdd1_autocomp/{ print $2 > "/sys/power/sr_vdd1_autocomp" }
	/sr_vdd2_autocomp/{ print $2 > "/sys/power/sr_vdd2_autocomp" }
	/clocks_off_while_idle/{ print $2 > "/sys/power/clocks_off_while_idle" }
	/voltage_off_while_idle/{ print $2 > "/sys/power/voltage_off_while_idle" }
	/scaling_governor/{ print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" }
	/scaling_max_freq/{ print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq" }
	/scaling_min_freq/{ print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq" }
