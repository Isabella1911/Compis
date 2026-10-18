global g_intentos, 4
global g_err, 4
global g_e, 4
string str_0, "error: "

func main, frame=0, temps=1
    g_intentos = 0
    try_begin L0
    g_intentos = g_intentos + 1
    try_end
    goto L1
L0:
    g_err = call __error_message, 0
    print_str g_err
L1:
L2:
    try_begin L4
    g_intentos = g_intentos + 1
    ifFalse g_intentos > 3 goto L6
    try_end
    goto L3
L6:
    try_end
    goto L5
L4:
    g_e = call __error_message, 0
    param g_e
    param str_0
    t0 = call __concat, 2
    print_str t0
L5:
    goto L2
L3:
    print_int g_intentos
endfunc
