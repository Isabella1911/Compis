global g_nums, 4
string str_0, "indice fuera de rango"

func main, frame=0, temps=2
    t0 = alloc 16
    t0[0] = 3
    t0[4] = 10
    t0[8] = 20
    t0[12] = 30
    g_nums = t0
    t0 = g_nums[0]
    if 1 < 0 goto L1
    if 1 < t0 goto L0
L1:
    param str_0
    call __raise, 1
L0:
    t0 = 1 * 4
    t0 = t0 + 4
    t1 = g_nums[t0]
    print_int t1
    t0 = g_nums[0]
    if 1 < 0 goto L3
    if 1 < t0 goto L2
L3:
    param str_0
    call __raise, 1
L2:
    t0 = 1 * 4
    t0 = t0 + 4
    g_nums[t0] = 99
    t0 = g_nums[0]
    if 1 < 0 goto L5
    if 1 < t0 goto L4
L5:
    param str_0
    call __raise, 1
L4:
    t0 = 1 * 4
    t0 = t0 + 4
    t1 = g_nums[t0]
    print_int t1
endfunc
