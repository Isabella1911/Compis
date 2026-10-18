global g_nota, 4
string str_0, "excelente"
string str_1, "aprobado"
string str_2, "reprobado"
string str_3, "F"
string str_4, "C"
string str_5, "A"

func main, frame=0, temps=0
    g_nota = 75
    ifFalse g_nota >= 90 goto L0
    print_str str_0
L0:
    ifFalse g_nota >= 60 goto L2
    print_str str_1
    goto L1
L2:
    print_str str_2
L1:
    ifFalse g_nota < 60 goto L4
    print_str str_3
    goto L3
L4:
    ifFalse g_nota < 80 goto L6
    print_str str_4
    goto L5
L6:
    print_str str_5
L5:
L3:
endfunc
