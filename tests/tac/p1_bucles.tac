global g_i, 4
global g_suma, 4
global g_j, 4

func main, frame=0, temps=1
    g_i = 0
    g_suma = 0
L0:
    ifFalse g_i < 10 goto L1
    g_i = g_i + 1
    t0 = g_i % 2
    ifFalse t0 == 0 goto L2
    goto L0
L2:
    ifFalse g_i > 7 goto L3
    goto L1
L3:
    g_suma = g_suma + g_i
    goto L0
L1:
L4:
    g_i = g_i - 1
L5:
    if g_i > 0 goto L4
L6:
    g_j = 0
L7:
    ifFalse g_j < 5 goto L9
    ifFalse g_j == 1 goto L10
    goto L8
L10:
    g_suma = g_suma + g_j
L8:
    g_j = g_j + 1
    goto L7
L9:
    print_int g_suma
endfunc
