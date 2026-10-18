func main, frame=0, temps=1
    param 5
    t0 = call fn_contador, 1
    print_int t0
endfunc

func fn_contador_siguiente, frame=0, temps=2
    t0 = fp[4]
    t1 = t0[8]
    t0 = t1 + 1
    return t0
endfunc

func fn_contador, frame=0, temps=1
    link fp
    t0 = call fn_contador_siguiente, 0
    return t0
endfunc
