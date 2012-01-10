# test delete file
rm /a
t_open cw /a
! abort --- Can't create file /a ---
set 9 $1
t_write_seq $9 100
! abort --- write file failed ---
t_close $9
! abort --- close file failed ---
rm /a
! abort --- can't delete /a ---
t_open cw /a
! abort --- Can't create file /a ---
set 9 $1
t_write_seq $9 100
! abort --- write file failed ---
rm /a
# this should failed, save $?(-1) to $8.
set 8 $?
t_close $9
! abort --- fail to close file ---
test $8 == -1
! abort --- can delete a file in use ? ---
echo --- test passed all ---
