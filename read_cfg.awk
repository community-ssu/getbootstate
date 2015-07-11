#!/usr/bin/awk -f
/enable_off_mode/		{ if (system("test -f /sys/power/enable_off_mode") == 0) { print $2 > "/sys/power/enable_off_mode" } }
/sleep_while_idle/		{ if (system("test -f /sys/power/sleep_while_idle") == 0) { print $2 > "/sys/power/sleep_while_idle" } }
/sr_vdd1_autocomp/		{ if (system("test -f /sys/power/sr_vdd1_autocomp") == 0) { print $2 > "/sys/power/sr_vdd1_autocomp" } }
/sr_vdd2_autocomp/		{ if (system("test -f /sys/power/sr_vdd2_autocomp") == 0) { print $2 > "/sys/power/sr_vdd2_autocomp" } }
/clocks_off_while_idle/		{ if (system("test -f /sys/power/clocks_off_while_idle") == 0) { print $2 > "/sys/power/clocks_off_while_idle" } }
/voltage_off_while_idle/	{ if (system("test -f /sys/power/voltage_off_while_idle") == 0) { print $2 > "/sys/power/voltage_off_while_idle" } }
/scaling_governor/		{ if (system("test -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor") == 0) { print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" } }
/scaling_max_freq/		{ if (system("test -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq") == 0) { print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq" } }
/scaling_min_freq/		{ if (system("test -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq") == 0) { print $2 > "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq" } }
/sleep_ind/			{
	if (system("cal-tool --get-rd-mode | grep -q -x 'enabled'") == 0) {
		if (system("test -f /sys/devices/platform/gpio-switch/sleep_ind/state") == 0) {
			if ($2 == 1) { VALUE="active" } else { VALUE="inactive" } print VALUE > "/sys/devices/platform/gpio-switch/sleep_ind/state"
		} else {
			if (system("test -f /sys/class/gpio/export -a ! -f /sys/class/gpio/gpio162/value") == 0) {
				print "162" > "/sys/class/gpio/export"
				system("sleep 0.1")
			}
			if (system("test -f /sys/class/gpio/gpio162/value") == 0) {
				print "out" > "/sys/class/gpio/gpio162/direction"
				print $2 > "/sys/class/gpio/gpio162/value"
			}
		}
	}
}
