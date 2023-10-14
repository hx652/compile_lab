define i32 @main(){
ENTRY:
    %0 = alloca i32
    %1 = alloca i32
    store i32 10, i32* %0
    store i32 0, i32* %1
    br label %LOOP

LOOP:
    %2 = load i32, i32* %1
    %3 = icmp ult i32 %2, 10
    br i1 %3, label %INNER, label %OUT

INNER:
    %4 = load i32, i32* %1
    %5 = add i32 %4, 1
    store i32 %5, i32* %1
    %6 = load i32, i32* %0
    %7 = load i32, i32* %1
    %8 = add i32 %6, %7
    store i32 %8, i32* %0
    br label %LOOP

OUT:
    %9 = load i32, i32* %0
    ret i32 %9
}