# Pyrkon-MPI

## Runnig the program
To run the program you need to have MPI installed on your machine. To compile the program use the following command:
```bash
mpicc pyrkon.c -o pyrkon.out  
```
To run the program use the following command:
```bash
mpirun -oversubscribe -np <num_of_processes> pyrkon.out <num_of_tickets> <num_of_workshops> <capacity_of_workshops>
```
