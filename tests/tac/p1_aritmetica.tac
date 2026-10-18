global g_a, 4
global g_b, 4
global g_c, 4

func main, frame=0, temps=1
    g_a = 7
    g_b = 3
    g_c = 2
    t0 = g_b * g_c
    t0 = g_a + t0
    print_int t0
    t0 = g_a - g_b
    t0 = t0 - g_c
    print_int t0
    t0 = g_a - g_b
    t0 = t0 * g_c
    print_int t0
    t0 = g_a / g_b
    t0 = t0 % g_c
    print_int t0
    t0 = -g_a
    t0 = t0 + g_b
    print_int t0
endfunc
