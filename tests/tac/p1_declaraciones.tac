global g_x, 4
global g_y, 4
global g_LIMITE, 4
global g_z, 4
global g_x_1, 4

func main, frame=0, temps=1
    g_x = 10
    g_y = 0
    g_LIMITE = 100
    g_z = 0
    g_y = g_x + 1
    g_y = g_x * 2
    g_z = g_y
    ifFalse g_x > 0 goto L0
    g_x_1 = 5
    print_int g_x_1
L0:
    print_int g_x
    print_int g_y
    print_int g_z
    print_int g_LIMITE
endfunc
