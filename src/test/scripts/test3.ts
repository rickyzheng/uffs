t_open cw /a
! abort --- create file /a failed.
set 9 $1

* 20 t_write_seq $9 50
! abort -- write failed

t_truncate $9 559
! abort -- truncate failed

t_close $9
! abort -- close failed

t_open r /a
! abort -- can't open /a
set 9 $1

t_check_seq $9 559
! abort -- check failed

t_close $9
