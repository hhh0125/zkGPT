import os
import r6b_resources
def check(row, mem):
    # Keep the raw snapshot; exempt only foreign jobs from this attempt's
    # occupancy veto. All existing capacity/cgroup/RLIMIT/PID checks remain.
    capacity = dict(row)
    warned = [x for x in row['visible_large_jobs'] if x['uid'] != os.getuid()]
    capacity['visible_large_jobs'] = [x for x in row['visible_large_jobs'] if x['uid'] == os.getuid()]
    r6b_resources.resource_check(capacity,mem,[])
    return warned
