global g_p, 4
vtable Punto_vtable, Punto_suma

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = Punto_vtable
    t0[4] = 0
    t0[8] = 0
    param 4
    param 3
    param t0
    call Punto_constructor, 3
    g_p = t0
    param g_p
    t0 = call Punto_suma, 1
    print_int t0
    t0 = g_p[4]
    print_int t0
endfunc

func Punto_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = x
    t0 = fp[8]
    t0[8] = y
endfunc

func Punto_suma, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[4]
    t1 = fp[8]
    t1 = t1[8]
    t0 = t0 + t1
    return t0
endfunc
