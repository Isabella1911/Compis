global g_dia, 4
global g_color, 4
string str_0, "lunes"
string str_1, "martes"
string str_2, "miercoles"
string str_3, "otro"
string str_4, "rojo"
string str_5, "verde"
string str_6, "alto"
string str_7, "siga"

func main, frame=0, temps=1
    g_dia = 3
    if g_dia == 1 goto L1
    if g_dia == 2 goto L2
    if g_dia == 3 goto L3
    goto L4
L1:
    print_str str_0
    goto L0
L2:
    print_str str_1
    goto L0
L3:
    print_str str_2
    goto L0
L4:
    print_str str_3
L0:
    g_color = str_4
    param str_4
    param g_color
    t0 = call __str_eq, 2
    if t0 goto L6
    param str_5
    param g_color
    t0 = call __str_eq, 2
    if t0 goto L7
    goto L5
L6:
    print_str str_6
    goto L5
L7:
    print_str str_7
    goto L5
L5:
endfunc
