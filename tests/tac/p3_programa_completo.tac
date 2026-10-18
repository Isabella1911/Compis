global g_r, 4
global g_numeros, 4
global g_suma, 4
global g_n, 4
global g_i, 4
global g_e, 4
global g_a, 4
global g_b, 4
string str_0, " calculada"
string str_1, "rectangulo"
string str_2, "correcto"
string str_3, "incorrecto"
string str_4, "indice fuera de rango"
string str_5, "indice fuera de rango: "
string str_6, "si"
string str_7, "no"
vtable Figura_vtable, Figura_area, Figura_describir
vtable Rectangulo_vtable, Rectangulo_area, Figura_describir

func main, frame=0, temps=4
    t0 = alloc 16
    t0[0] = Rectangulo_vtable
    t0[4] = 0
    t0[8] = 0
    t0[12] = 0
    param 4
    param 3
    param t0
    call Rectangulo_constructor, 3
    g_r = t0
    param g_r
    t0 = call Figura_describir, 1
    print_str t0
    t0 = alloc 24
    t0[0] = 5
    t0[4] = 1
    t0[8] = 2
    t0[12] = 3
    t0[16] = 4
    t0[20] = 5
    g_numeros = t0
    g_suma = 0
    t0 = g_numeros[0]
    t1 = 0
L1:
    ifFalse t1 < t0 goto L3
    t2 = t1 * 4
    t2 = t2 + 4
    g_n = g_numeros[t2]
    t2 = g_n % 2
    ifFalse t2 == 0 goto L4
    goto L2
L4:
    t2 = g_n * g_n
    g_suma = g_suma + t2
L2:
    t1 = t1 + 1
    goto L1
L3:
    print_int g_suma
    g_i = 0
L5:
    param g_i
    t0 = call fn_fibonacci, 1
    print_int t0
    g_i = g_i + 1
L6:
    if g_i < 6 goto L5
L7:
    if g_suma == 35 goto L9
    goto L10
L9:
    print_str str_2
    goto L8
L10:
    print_str str_3
L8:
    try_begin L11
    t0 = g_numeros[0]
    if 10 < 0 goto L14
    if 10 < t0 goto L13
L14:
    param str_4
    call __raise, 1
L13:
    t0 = 10 * 4
    t0 = t0 + 4
    t1 = g_numeros[t0]
    print_int t1
    try_end
    goto L12
L11:
    g_e = call __error_message, 0
    param g_e
    param str_5
    t0 = call __concat, 2
    print_str t0
L12:
    param 1
    t0 = call fn_crearAcumulador, 1
    print_int t0
    g_a = 2
    g_b = 3
    t0 = g_a * g_b
    t1 = g_a - g_b
    t2 = g_a + g_b
    t1 = t1 * t2
    t1 = t1 / 2
    t0 = t0 + t1
    ifFalse t0 >= 0 goto L15
    if g_a == g_b goto L15
    t0 = str_6
    goto L16
L15:
    t0 = str_7
L16:
    print_str t0
endfunc

func Figura_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = nombre
endfunc

func Figura_area, frame=0, temps=0
    return 0
endfunc

func Figura_describir, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    param str_0
    param t0
    t0 = call __concat, 2
    return t0
endfunc

func Rectangulo_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = str_1
    t0 = fp[8]
    t0[8] = ancho
    t0 = fp[8]
    t0[12] = alto
endfunc

func Rectangulo_area, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[8]
    t1 = fp[8]
    t1 = t1[12]
    t0 = t0 * t1
    return t0
endfunc

func fn_fibonacci, frame=0, temps=2
    ifFalse n < 2 goto L0
    return n
L0:
    t0 = n - 1
    param t0
    t0 = call fn_fibonacci, 1
    t1 = n - 2
    param t1
    t1 = call fn_fibonacci, 1
    t0 = t0 + t1
    return t0
endfunc

func fn_crearAcumulador_sumar, frame=0, temps=2
    t0 = fp[4]
    t1 = t0[-8]
    t0 = t1 + x
    t1 = fp[4]
    t1[-8] = t0
    t0 = fp[4]
    t1 = t0[-8]
    return t1
endfunc

func fn_crearAcumulador, frame=4, temps=1
    total = inicio
    param 5
    link fp
    t0 = call fn_crearAcumulador_sumar, 1
    param 10
    link fp
    t0 = call fn_crearAcumulador_sumar, 1
    return t0
endfunc
