platform = Win16
CC = wcc386
CFLAGS = -zW
res1_flags = -bt=windows -30

!include ../quake31.mif
!include ../master.mif

linkit : .PROCEDURE
    @%append $(LNK) system win386
    @%append $(LNK) option ELIMINATE
    @%append $(LNK) option mindata=0
    @%append $(LNK) option stack=256k

bindit : .PROCEDURE
    wbind -q $(name) -R $(name).res
