global g_a, 4
global g_b, 4
global g_mayor, 4
global g_texto, 4
string str_0, "b gana"
string str_1, "a gana"

func main, frame=0, temps=1
    g_a = 4
    g_b = 9
    ifFalse g_a > g_b goto L0
    t0 = g_a
    goto L1
L0:
    t0 = g_b
L1:
    g_mayor = t0
    ifFalse g_mayor == g_b goto L2
    t0 = str_0
    goto L3
L2:
    t0 = str_1
L3:
    g_texto = t0
    print_int g_mayor
    print_str g_texto
    ifFalse g_a < g_b goto L4
    ifFalse g_b < 10 goto L4
    t0 = 1
    goto L5
L4:
    t0 = 0
L5:
    print_int t0
endfunc
