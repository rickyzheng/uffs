#export UFFS_TEST_SRV_ADDR="192.168.0.103"
export UFFS_TEST_SRV_ADDR="127.0.0.1"
mkdir -p bak && rm -f bak/*
time ./testfixture test-uffs/all.test 2>&1 | tee test-uffs.log
