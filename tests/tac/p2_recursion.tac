func main, frame=0, temps=1
    param 5
    t0 = call fn_fact, 1
    print_int t0
endfunc

func fn_fact, frame=0, temps=1
    ifFalse n <= 1 goto L0
    return 1
L0:
    t0 = n - 1
    param t0
    t0 = call fn_fact, 1
    t0 = n * t0
    return t0
endfunc
