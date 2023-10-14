define i32 @main(){
ENTRY:
    %0 = alloca float
    store float 0x40163851E0000000, float* %0
    %1 = sitofp i32 1 to float
    %2 = load float, float* %0
    %3 = fcmp ugt float %2, %1
    br i1 %3, label %IFTRUE, label %IFFALSE
    
IFTRUE:
    ret i32 233

IFFALSE:
    ret i32 0
}