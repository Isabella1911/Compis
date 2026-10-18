global g_p, 4
string str_0, "Toby"
vtable Animal_vtable, Animal_hablar
vtable Perro_vtable, Animal_hablar, Perro_ladrar

func main, frame=0, temps=1
    t0 = alloc 8
    t0[0] = Perro_vtable
    t0[4] = 0
    param str_0
    param t0
    call Animal_constructor, 2
    g_p = t0
    param g_p
    t0 = call Animal_hablar, 1
    print_str t0
    param g_p
    t0 = call Perro_ladrar, 1
    print_str t0
endfunc

func Animal_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = nombre
endfunc

func Animal_hablar, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    return t0
endfunc

func Perro_ladrar, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    return t0
endfunc
