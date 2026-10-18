global g_a, 4
global g_b, 4
global g_c, 4
global g_d, 4
global g_e, 4
global g_f, 4
global g_r, 4
global g_s, 4

func main, frame=0, temps=2
    g_a = 1
    g_b = 2
    g_c = 3
    g_d = 4
    g_e = 5
    g_f = 6
    t0 = g_a * g_b
    t1 = g_c * g_d
    t0 = t0 + t1
    t1 = g_e * g_f
    g_r = t0 - t1
    t0 = g_a + g_b
    t1 = g_c + g_d
    t0 = t0 * t1
    t1 = g_e + g_f
    g_s = t0 * t1
    t0 = g_r + g_s
    print_int t0
endfunc
