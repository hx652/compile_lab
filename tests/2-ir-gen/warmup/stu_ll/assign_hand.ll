define i32 @main(){
ENTRY:
    ; int a[10]
    %0 = alloca [10 x i32]
    
    ; a[0] = 10
    %1 = getelementptr [10 x i32], [10 x i32]* %0, i32 0, i32 0
    store i32 10, i32* %1
    
    ; a[1] = a[0] * 2
    %2 = load i32, i32* %1
    %3 = mul i32 %2, 2
    %4 = getelementptr [10 x i32], [10 x i32]* %0, i32 0, i32 1
    store i32 %3, i32* %4

    ; return a[1]
    %5 = load i32, i32* %4
    ret i32 %5
}