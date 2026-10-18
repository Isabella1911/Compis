global g_nums, 4
global g_total, 4
global g_n, 4

func main, frame=0, temps=4
    t0 = alloc 16
    t0[0] = 3
    t0[4] = 1
    t0[8] = 2
    t0[12] = 3
    g_nums = t0
    g_total = 0
    t0 = g_nums[0]
    t1 = 0
L0:
    ifFalse t1 < t0 goto L2
    t2 = t1 * 4
    t2 = t2 + 4
    g_n = g_nums[t2]
    g_total = g_total + g_n
L1:
    t1 = t1 + 1
    goto L0
L2:
    print_int g_total
endfunc
