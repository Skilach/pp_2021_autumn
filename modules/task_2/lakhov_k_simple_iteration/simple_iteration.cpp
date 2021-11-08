// Copyright 2021 Lakhov Kirill

#include <vector>
#include <cstdlib>
#include "../../../modules/task_2/lakhov_k_simple_iteration/simple_iteration.h"

std::vector<double> getRandomVector(int sz, int min, int max) {
    std::random_device rd;
    std::default_random_engine engine(rd());
    std::uniform_real_distribution<double> distribution(min, max);
    std::vector<double> vec(sz);
    for (int  i = 0; i < sz; i++) { vec[i] = distribution(engine); }
    return vec;
}

std::vector<std::vector<double>> getRandomMatrix(int size) {
    std::vector<std::vector<double>> vec(size);
    std::vector<double> prevailing = getRandomVector(size, 80, 100);
    for (int  i = 0; i < size; i++) {
        vec[i] = getRandomVector(size, -10, 10);
        vec[i][i] = prevailing[i];
    }
    return vec;
}

void printMatrix(std::vector<std::vector<double>> m, int size){
    for(int i=0; i<size; i++){
        for(int j=0; j<size; j++){
            std::cout << m[i][j]<<" ";
        }
        std::cout << std::endl;
    }
}

void printVector(std::vector<double> m, int size){
    for(int i=0; i<size; i++){
        std::cout << m[i]<<" ";
    }
    std::cout << std::endl;
}


/*
    Ax=b
    [a(1,1), a(1,2)] x [x(1)] = [b(1)]
    [a(2,1), a(2,2)]   [x(2)]   [b(2)]

    Ax=b -> x = Cx + d

    *(-1) if a(i, i) < 0
    : -a(i, i)

    [x(1)] = [0, -a(1,2)/a(1,1)] x  [x(1)] + [b(1)/a(1,1)]
    [x(2)]   [-a(2,1)/a(2,2), 0]    [x(2)]   [b(2)/a(2,2)]


    [x(k+1)] = [0, -a(1,2)/a(1,1)] x  [x(k)] + [b(1)/a(1,1)]
    [x(k+1)]   [-a(2,1)/a(2,2), 0]    [x(k)]   [b(2)/a(2,2)]

    условие сходимости:
    max[i](sum[j=1, n](|C[i][j])|) < 1
    обеспечиваем a(i,i) > sum[j=1, n, j!=i](a(i, j))
*/

std::vector<double> sequentialCalc(std::vector<std::vector<double>> coefficients,
                                   std::vector<double> free_members, int size) {
    for(int i = 0; i < size; i++){
        double coeff = coefficients[i][i];
        bool coeff_is_negative = coeff < 0;
        free_members[i] /= coeff_is_negative ? -coeff : coeff;
        for(int j = 0; j < size; j++){
            if(i != j){
                coefficients[i][j] /= coeff_is_negative ? coeff : -coeff;
            }else{
                coefficients[i][j] = 0;
            }
        }
    }
    // printMatrix(coefficients, size);
    // printVector(free_members, size);

    std::vector<double> x(size);
    std::vector<double> next_x = free_members;
    double epsilon = 0.0001;
    bool stop = false;
    // int step = 1;
    while(!stop){
        for(int i=0; i<size; i++){
            x[i] = next_x[i];
            next_x[i] = 0;
        }
        for(int i = 0; i<size; i++){
            for(int j=0; j<size; j++){
                next_x[i] += coefficients[i][j]*x[j];
            }
            next_x[i] += free_members[i];
        }

        stop = true;
        for(int i = 0; i < size - 1; i++){
            if(std::abs(x[i] - next_x[i]) > epsilon){
                stop = false;
            }
        }
        // std::cout<<"step: "<<step<<std::endl;
        // step++;
        // printVector(x, size);
        // printVector(next_x, size);
        // if(step>10){
        //     stop = true;
        // }
    }
    return next_x;
}


int parallelCalc(std::vector<std::vector<double>> matrix, std::vector<double> free_members, int size) {
    int proc_count = 0, proc_rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &proc_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);

    int rows_per_proc = size / proc_count;
    int excess_rows = size % proc_count;

    int amount_of_elems_per_proc = rows_per_proc*size+rows_per_proc;
    int current_amount_of_elems = amount_of_elems_per_proc;
    if(proc_rank < excess_rows){
        current_amount_of_elems += size + 1;
    }
    // int row_len = size+1;

    std::vector<double> raw_data;
    std::vector<int> send_counts(proc_count, amount_of_elems_per_proc);
    std::vector<int> send_offsets(proc_count, 0);
    std::vector<double> proc_data(current_amount_of_elems);

    if(proc_rank == 0){
        printMatrix(matrix, size);
        printVector(free_members, size);
        // flatten vector and add free members for each row
        std::size_t total_size = size*size + size;
        raw_data.reserve(total_size);
        for(int i = 0; i < size; i++){
            matrix[i].push_back(free_members[i]);
            raw_data.insert(raw_data.end(), matrix[i].begin(), matrix[i].end());
        }

        for (int i = 0; i < excess_rows; i++)
            send_counts[i] += size + 1;
        for (int i = 1; i < proc_count; i++)
            send_offsets[i] = send_offsets[i - 1] + send_counts[i - 1];
    }
    MPI_Scatterv(raw_data.data(), send_counts.data(), send_offsets.data(),
                MPI_DOUBLE, proc_data.data(), amount_of_elems_per_proc, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    std::cout<<"proc #"<<proc_rank<<std::endl;
    printVector(proc_data, current_amount_of_elems);

    int start_x_index = rows_per_proc*proc_rank+std::min(proc_rank, excess_rows);
    int x_index = start_x_index;
    int row_size = size + 1;
    for(int i = 0; i < current_amount_of_elems; i += row_size){
        double coeff = proc_data[x_index + i*row_size];
        bool coeff_is_negative = coeff < 0;
        int free_member_index = (i+1)*row_size - 1;
        proc_data[free_member_index] /= coeff_is_negative ? -coeff : coeff;
        // handle everything except last number that is not a coefficient
        for(int j = i; j < i + row_size - 1; j++){
            if(j != x_index + i * row_size){
                proc_data[j] /= coeff_is_negative ? coeff : -coeff;
            }else{
                proc_data[j] = 0;
            }
        }
        x_index++;
    }
    std::cout<<"proc #"<<proc_rank<<std::endl;
    printVector(proc_data, current_amount_of_elems);

    std::vector<double> x(size);
    std::vector<double> this_x;
    for(int i = 0; i < current_amount_of_elems; i += row_size){
        int free_member_index = (i+1)*row_size - 1;
        this_x.push_back(proc_data[free_member_index]);
    }

    send_counts.assign()
    MPI_Allgatherv(this_x.data(), (int)(this_x.size()), MPI_DOUBLE, x.data(), size, MPI_DOUBLE,
                MPI_COMM_WORLD);
    if(proc_rank==0){
        std::cout<<"gather #0"<<proc_rank<<std::endl;
        printVector(x, size);
    }
    return 1;
}
