#!/usr/bin/env bash
# Entry point for the image bench.Dockerfile builds. Prints what the image is
# before it prints any number, then hands the arguments to bench.sh.
set -uo pipefail

echo
echo "  ENVIRONMENT"
echo
sed 's/^/  /' /etc/bench-environment
allowed=$(taskset -pc $$ 2>/dev/null | sed 's/.*: *//')
printf '  cpus available  %s (may use: %s)' "$(nproc)" "${allowed:-unknown}"
[ -r /sys/fs/cgroup/cpu.max ] && printf '   cgroup cpu.max %s' "$(cat /sys/fs/cgroup/cpu.max)"
printf '\n  load average    %s (of the host, not of this container)\n' "$(cut -d' ' -f1-3 /proc/loadavg)"

# taskset and nice are what bench.sh uses to hold the measurement still. Both
# can be missing capabilities in a container; say so here rather than let the
# numbers quietly get noisier. bench.sh picks its cores out of the set above,
# so a cpuset of any shape works, but it needs at least two to time the
# programs that create tasks the way the published figures were timed.
[ -n "$allowed" ] \
    || echo "  WARNING: cannot read this process's cpu affinity, pinning may not hold"
nice -n -5 true 2>/dev/null \
    || echo "  NOTE: no CAP_SYS_NICE, measuring at normal priority (add --cap-add=SYS_NICE)"
echo

exec bash /bench/bench.sh "$@"
