cd .. && make clean all PGSIZE=$1 && cd benchmark && make clean all && for i in {1..1000}; do ./mtest >> $1.txt && python avg.py $1; done