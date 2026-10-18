global g_xs, 4
string str_0, "indice fuera de rango"

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = 2
    t0[4] = 1
    t0[8] = 2
    g_xs = t0
    param g_xs
    t0 = call fn_busca, 1
    print_int t0
endfunc

func fn_busca, frame=4, temps=2
    try_begin L0
    t0 = xs[0]
    if 0 < 0 goto L4
    if 0 < t0 goto L3
L4:
    param str_0
    call __raise, 1
L3:
    t0 = 0 * 4
    t0 = t0 + 4
    t1 = xs[t0]
    ifFalse t1 == 1 goto L2
    try_end
    return 1
L2:
    try_end
    goto L1
L0:
    e = call __error_message, 0
    print_str e
L1:
    return 0
endfunc
