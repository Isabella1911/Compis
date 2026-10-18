global g_f, 4
vtable Figura_vtable, Figura_area, Figura_describir
vtable Cuadrado_vtable, Cuadrado_area, Figura_describir

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = Cuadrado_vtable
    t0[4] = 0
    t0[8] = 0
    param 3
    param t0
    call Cuadrado_constructor, 2
    g_f = t0
    param g_f
    t0 = g_f[0]
    t0 = t0[0]
    t0 = call t0, 1
    print_int t0
    param g_f
    t0 = call Figura_describir, 1
    print_int t0
    t0 = alloc 8
    t0[0] = Figura_vtable
    t0[4] = 0
    param 3
    param t0
    call Figura_constructor, 2
    param t0
    t0 = call fn_medir, 1
    print_int t0
endfunc

func Figura_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = lados
endfunc

func Figura_area, frame=0, temps=0
    return 0
endfunc

func Figura_describir, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[4]
    t0 = t0 * 100
    t1 = fp[8]
    param t1
    t1 = t1[0]
    t1 = t1[0]
    t1 = call t1, 1
    t0 = t0 + t1
    return t0
endfunc

func Cuadrado_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = 4
    t0 = fp[8]
    t0[8] = lado
endfunc

func Cuadrado_area, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[8]
    t1 = fp[8]
    t1 = t1[8]
    t0 = t0 * t1
    return t0
endfunc

func fn_medir, frame=0, temps=1
    param f
    t0 = f[0]
    t0 = t0[0]
    t0 = call t0, 1
    return t0
endfunc
