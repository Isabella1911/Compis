global g_a, 4
global g_b, 4
global g_enRango, 4
global g_alguno, 4
global g_ninguno, 4
string str_0, "si"
string str_1, "distintos"

func main, frame=0, temps=1
    g_a = 5
    g_b = 10
    ifFalse g_a > 0 goto L0
    ifFalse g_a < g_b goto L0
    t0 = 1
    goto L1
L0:
    t0 = 0
L1:
    g_enRango = t0
    if g_a == 0 goto L4
    ifFalse g_b == 0 goto L2
L4:
    t0 = 1
    goto L3
L2:
    t0 = 0
L3:
    g_alguno = t0
    g_ninguno = !g_alguno
    ifFalse g_a < g_b goto L5
    if g_b < 20 goto L6
    if g_enRango goto L5
L6:
    print_str str_0
L5:
    if g_a == g_b goto L7
    print_str str_1
L7:
    print_bool g_enRango
L8:
    print_bool g_ninguno
endfunc
