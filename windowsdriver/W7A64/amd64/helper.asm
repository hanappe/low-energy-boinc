.data


.code

MsrValueHi$ = 32
MsrValueLo$ = 36
MsrValue$ = 40
RdMsrId$ = 64
RdMsr64 PROC
mov		DWORD PTR [rsp+8], ecx
sub		rsp, 56			
mov		QWORD PTR MsrValue$[rsp], 0
mov		DWORD PTR MsrValueLo$[rsp], 0
mov		DWORD PTR MsrValueHi$[rsp], 0 
mov		ecx, DWORD PTR RdMsrId$[rsp]
rdmsr
mov     DWORD PTR MsrValueLo$[rsp], Eax 
mov     DWORD PTR MsrValueHi$[rsp], Edx
mov		eax, DWORD PTR MsrValueLo$[rsp]
mov		QWORD PTR MsrValue$[rsp], rax
mov		eax, DWORD PTR MsrValueHi$[rsp]
mov		DWORD PTR MsrValue$[rsp+4], eax
mov		rax, QWORD PTR MsrValue$[rsp]
add		rsp, 56					
ret		0
RdMsr64 ENDP



WrMsr64ExINTEL PROC
mov		DWORD PTR [rsp+8], ecx
sub		rsp, 24	
mov		ecx, 198h
rdmsr
and		edx, 1h
mov		ecx, 199h
mov     eax, DWORD PTR [rsp+32]
wrmsr
add		rsp, 24					
ret		0
WrMsr64ExINTEL ENDP

WrMsr64ExAMD PROC
mov		DWORD PTR [rsp+8], ecx
sub		rsp, 24	
mov		edx, 0h
mov		ecx, 0C0010062h
mov     eax, DWORD PTR [rsp+32]
wrmsr
add		rsp, 24					
ret		0
WrMsr64ExAMD ENDP
end