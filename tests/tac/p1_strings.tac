global g_nombre, 4
global g_saludo, 4
global g_igual, 4
global g_distinto, 4
string str_0, "Compi"
string str_1, "hola "
string str_2, "hola Compi"
string str_3, "null"

func main, frame=0, temps=1
    g_nombre = str_0
    param g_nombre
    param str_1
    g_saludo = call __concat, 2
    param str_2
    param g_saludo
    g_igual = call __str_eq, 2
    param str_0
    param g_nombre
    t0 = call __str_eq, 2
    g_distinto = !t0
    print_str g_saludo
    print_bool g_igual
    print_bool g_distinto
    print_str str_1
    print_str str_3
endfunc
