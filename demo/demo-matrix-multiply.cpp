//
// Created by dingjing on 8/25/22.
//

#include <vector>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

namespace algorithm
{
    typedef std::vector<std::vector<double>> Matrix;

    struct MMInput
    {
        Matrix a;
        Matrix b;
    };

    struct MMOutput
    {
        int error;
        size_t m, n, k;
        Matrix c;
    };

    bool is_valid_matrix(const Matrix& matrix, size_t& m, size_t& n)
    {
        m = n = 0;
        if (matrix.size() == 0)
            return true;

        m = matrix.size();
        n = matrix[0].size();
        if (n == 0)
            return false;

        for (const auto& row : matrix)
            if (row.size() != n)
                return false;

        return true;
    }

    void matrix_multiply(const MMInput *in, MMOutput *out)
    {
        size_t m1, n1;
        size_t m2, n2;

        if (!is_valid_matrix(in->a, m1, n1) || !is_valid_matrix(in->b, m2, n2)) {
            out->error = EINVAL;
            return;
        }

        if (n1 != m2) {
            out->error = EINVAL;
            return;
        }

        out->error = 0;
        out->m = m1;
        out->n = n2;
        out->k = n1;

        out->c.resize(m1);
        for (size_t i = 0; i < out->m; i++) {
            out->c[i].resize(n2);
            for (size_t j = 0; j < out->n; j++) {
                out->c[i][j] = 0;
                for (size_t k = 0; k < out->k; k++) {
                    out->c[i][j] += in->a[i][k] * in->b[k][j];
                }
            }
        }
    }

}

using MMTask = ThreadTask<algorithm::MMInput, algorithm::MMOutput>;

using namespace algorithm;

void print_matrix(const Matrix& matrix, size_t m, size_t n)
{
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++)
            printf("\t%8.2lf", matrix[i][j]);

        printf("\n");
    }
}

void callback(MMTask *task)
{
    auto *input = task->getInput();
    auto *output = task->getOutput();

    assert(task->getState() == TASK_STATE_SUCCESS);

    if (output->error)
        printf("Error: %d %s\n", output->error, strerror(output->error));
    else {
        printf("Matrix A\n");
        print_matrix(input->a, output->m, output->k);
        printf("Matrix B\n");
        print_matrix(input->b, output->k, output->n);
        printf("Matrix A * Matrix B =>\n");
        print_matrix(output->c, output->m, output->n);
    }
}

int main()
{
    using MMFactory = ThreadTaskFactory<MMInput, MMOutput>;
    MMTask *task = MMFactory::createThreadTask("matrix_multiply_task", matrix_multiply, callback);
    auto *input = task->getInput();

    input->a = {{1, 2, 3}, {4, 5, 6}};
    input->b = {{7, 8}, {9, 10}, {11, 12}};

    Facilities::WaitGroup wait_group(1);

    Workflow::startSeriesWork(task, [&wait_group](const SeriesWork *) {
        wait_group.done();
    });

    wait_group.wait();
    return 0;
}

