/*
Copyright (C) 2003-2015 CompatibL

This file is part of TapeScript, an open source library and tape encoding
standard for adjoint algorithmic differentiation (AAD), available from

    http://github.com/compatibl/tapescript (source)
    http://tapescript.org (documentation)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef cl_tape_examples_impl_quadratic_regression_examples_hpp
#define cl_tape_examples_impl_quadratic_regression_examples_hpp

#define CL_BASE_SERIALIZER_OPEN
#include <cl/tape/tape.hpp>

namespace cl
{
    struct quadratic_initial_data
    {
        // Control production of tape output to serializer.
        bool flag_serializer = false;

        // Input quadratic regression parameters.
        // n: number of points {x_i, y_i}.
        // x_i are x_0 = 0, x_1 = 1, ..., x_n = n.
        // y_i are y_i = a + b * x_i + c * x_i * x_i + exp(-1 * d * x_i).
#if defined NDEBUG
        size_t n = 10000;
#else
        size_t n = 2000;
#endif
        double a = 1.0;
        double b = 2.0;
        double c = 3.0;
        double d = 4.0;

        // Tolerance for analytical vs. adjoint derivative check.
        double eps = 1e-10;
    };

    // Example of quadratic regression differentiation with respect to parameters of input distribution using optimized tape.
    inline void quadratic_regression_with_params_optimized_example(const quadratic_initial_data& data, std::ostream& out_str = std::cout)
    {
        // Control production of tape output to serializer.
        bool flag_serializer = data.flag_serializer;

        // Input linear regression parameters.
        // n: number of points {x_i, y_i}.
        // x_i are x_0 = 0, x_1 = 1, ..., x_n = n.
        // y_i are y_i = a + b * x_i + c * x_i * x_i + exp(-1 * d * x_i).
        size_t n = data.n;
        double a = data.a;
        double b = data.b;
        double c = data.c;
        double d = data.d;

        // Tolerance for analytical vs. adjoint derivative check.
        double eps = data.eps;

        out_str << "Quadratic regression with parameters (optimized tape):\n" << std::endl;

        // Input values initialization.
        out_str << "Input vector size: n = " << n << std::endl;
        cl::InnerArray a_ref = a;
        cl::InnerArray b_ref = b;
        cl::InnerArray c_ref = c;
        cl::InnerArray d_ref = d;
        std::vector<cl::tape_array> X = { a_ref, b_ref, c_ref, d_ref };
        if (flag_serializer)
            out_str << "Input vector: " << X << "\n";

        std::clock_t start_time = std::clock();
        // Declare the X vector as independent and start a tape recording.
        cl::Independent(X);

        // Output calculations.
        cl::tape_array& par_a = X[0];
        cl::tape_array& par_b = X[1];
        cl::tape_array& par_c = X[2];
        cl::tape_array& par_d = X[3];
        // Obtain x_i values.
        std::valarray<double> x_ref(n);
        for (int i = 0; i < n; i++)
            x_ref[i] = i;
        cl::tape_array x = x_ref;
        cl::tape_array x2 = x * x;
        // Calculate corresponding y_i values.
        cl::tape_array y = par_a + x * par_b + x2 * par_c + std::exp(-1 * par_d * x);
        // Start quadratic regression calculation: calculate mean values.
        cl::tape_array sum_x = cl::tapescript::sum_vec(x);
        cl::tape_array sum_y = cl::tapescript::sum_vec(y);
        cl::tape_array sum_x2 = cl::tapescript::sum_vec(x2);
        cl::tape_array y2 = y * y;
        cl::tape_array sum_y2 = cl::tapescript::sum_vec(y2);
        cl::tape_array xy = x * y;
        cl::tape_array sum_xy = cl::tapescript::sum_vec(xy);
        cl::tape_array x3 = x2 * x;
        cl::tape_array sum_x3 = cl::tapescript::sum_vec(x3);
        cl::tape_array x2y = x2 * y;
        cl::tape_array sum_x2y = cl::tapescript::sum_vec(x2y);
        cl::tape_array x4 = x3 * x;
        cl::tape_array sum_x4 = cl::tapescript::sum_vec(x4);
        // Calculate covariances.
        cl::tape_array S_xx = sum_x2 - sum_x * sum_x / n;
        cl::tape_array S_xy = sum_xy - sum_x * sum_y / n;
        cl::tape_array S_xx2 = sum_x3 - sum_x * sum_x2 / n;
        cl::tape_array S_x2y = sum_x2y - sum_x2 * sum_y / n;
        cl::tape_array S_x2x2 = sum_x4 - sum_x2 * sum_x2 / n;
        // Quadratic regression coefficients.
        cl::tape_array denominator = S_xx * S_x2x2 - pow(S_xx2, 2.0);
        cl::tape_array gamma = (S_x2y * S_xx - S_xy * S_xx2) / denominator;
        cl::tape_array beta = (S_xy * S_x2x2 - S_x2y * S_xx2) / denominator;
        cl::tape_array alpha = (sum_y - beta * sum_x - gamma * sum_x2) / n;
        // Estimation for y_i.
        cl::tape_array y_estimate = alpha + beta * x + gamma * x2;
        // Output vector.
        std::vector<cl::tape_array> Y = { alpha, beta, gamma, y_estimate };
        //out_str << "Output vector: " << Y << "\n\n";

        if (flag_serializer)
            out_str << "Ininial Forward(0) sweep...\n\n";
        // Declare a tape function and stop the tape recording.
        cl::tape_function<cl::InnerArray> f(X, Y);
        std::clock_t stop_time = std::clock();
        out_str << "Tape memory (bytes): " << f.Memory() << std::endl;
        out_str << "Tape creation took (ms): " << (stop_time - start_time) / (double)(CLOCKS_PER_SEC)* 1000 << '\n';

        // Forward sweep calculations.
        std::valarray<double> d_ref_array;
        d_ref_array.resize(3);

        // Derivative calculation time.
        std::clock_t calc_time = 0;

        // Derivatives with respect to a.
        std::vector<cl::InnerArray> dx = { 1, 0, 0, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        std::vector<cl::InnerArray> forw;
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to b.
        dx = { 0, 1, 0, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to c.
        dx = { 0, 0, 1, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to d.
        dx = { 0, 0, 0, 1 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        out_str << "Forward calculation took (ms): " << calc_time / (double)(CLOCKS_PER_SEC)* 1000 << '\n';
        out_str << std::endl;
    }

    // Example of quadratic regression differentiation with respect to parameters of input distribution using optimized tape.
    inline void quadratic_regression_with_params_nonoptimized_example(const quadratic_initial_data& data, std::ostream& out_str = std::cout)
    {
        // Control production of tape output to serializer.
        bool flag_serializer = data.flag_serializer;

        // Input linear regression parameters.
        // n: number of points {x_i, y_i}.
        // x_i are x_0 = 0, x_1 = 1, ..., x_n = n.
        // y_i are y_i = a + b * x_i + c * x_i * x_i + exp(-1 * d * x_i).
        size_t n = data.n;
        double a = data.a;
        double b = data.b;
        double c = data.c;
        double d = data.d;

        // Tolerance for analytical vs. adjoint derivative check.
        double eps = data.eps;

        out_str << "Quadratic regression with parameters (non-optimized tape):\n" << std::endl;

        // Input values initialization.
        out_str << "Input vector size: n = " << n << std::endl;
        cl::tape_double_vector X = { a, b, c, d };
        if (flag_serializer)
            out_str << "Input vector: " << X << "\n";

        std::clock_t start_time = std::clock();
        // Declare the X vector as independent and start a tape recording.
        cl::Independent(X);

        // Output calculations.
        cl::tape_double& par_a = X[0];
        cl::tape_double& par_b = X[1];
        cl::tape_double& par_c = X[2];
        cl::tape_double& par_d = X[3];
        // Obtain x_i values.
        cl::tape_double_vector x(n);
        for (int i = 0; i < n; i++)
            x[i] = i;
        cl::tape_double_vector x2(n);
        for (int i = 0; i < n; i++) 
            x2[i] = x[i] * x[i];
        // Calculate corresponding y_i values.
        cl::tape_double_vector y(n);
        for (int i = 0; i < n; i++)
            y[i] = par_a + x[i] * par_b + x2[i] * par_c + std::exp(-1 * par_d * x[i]);
        // Start quadratic regression calculation: calculate mean values.
        cl::tape_double sum_x = 0.0;
        for (int i = 0; i < n; i++)
            sum_x += x[i];
        cl::tape_double sum_y = 0.0;
        for (int i = 0; i < n; i++)
            sum_y += y[i];
        cl::tape_double sum_x2 = 0.0;
        for (int i = 0; i < n; i++)
            sum_x2 += x2[i];
        cl::tape_double_vector y2(n);
        for (int i = 0; i < n; i++)
            y2[i] = y[i] * y[i];
        cl::tape_double sum_y2 = 0.0;
        for (int i = 0; i < n; i++)
            sum_y2 += y2[i];
        cl::tape_double_vector xy(n);
        for (int i = 0; i < n; i++) 
            xy[i] = x[i] * y[i];
        cl::tape_double sum_xy = 0.0;
        for (int i = 0; i < n; i++)
            sum_xy += xy[i];
        cl::tape_double_vector x3(n);
        for (int i = 0; i < n; i++) 
            x3[i] = x2[i] * x[i];
        cl::tape_double sum_x3 = 0.0;
        for (int i = 0; i < n; i++)
            sum_x3 += x3[i];
        cl::tape_double_vector x2y(n);
        for (int i = 0; i < n; i++) 
            x2y[i] = x2[i] * y[i];
        cl::tape_double sum_x2y = 0.0;
        for (int i = 0; i < n; i++)
            sum_x2y += x2y[i];
        cl::tape_double_vector x4(n);
        for (int i = 0; i < n; i++) 
            x4[i] = x3[i] * x[i];
        cl::tape_double sum_x4 = 0.0;
        for (int i = 0; i < n; i++)
            sum_x4 += x4[i];
        // Calculate covariances.
        cl::tape_double S_xx = sum_x2 - sum_x * sum_x / n;
        cl::tape_double S_xy = sum_xy - sum_x * sum_y / n;
        cl::tape_double S_xx2 = sum_x3 - sum_x * sum_x2 / n;
        cl::tape_double S_x2y = sum_x2y - sum_x2 * sum_y / n;
        cl::tape_double S_x2x2 = sum_x4 - sum_x2 * sum_x2 / n;
        // Quadratic regression coefficients.
        cl::tape_double denominator = S_xx * S_x2x2 - std::pow(S_xx2, 2.0);
        cl::tape_double gamma = (S_x2y * S_xx - S_xy * S_xx2) / denominator;
        cl::tape_double beta = (S_xy * S_x2x2 - S_x2y * S_xx2) / denominator;
        cl::tape_double alpha = (sum_y - beta * sum_x - gamma * sum_x2) / n;
        // Estimation for y_i.
        cl::tape_double_vector y_estimate(n);
        for (int i = 0; i < n; i++)
           y_estimate[i] = alpha + beta * x[i] + gamma * x2[i];
        // Output vector.
        cl::tape_double_vector Y(n + 3);
        Y[0] = alpha;
        Y[1] = beta;
        Y[2] = gamma;
        for (int i = 0; i < n; i++)
            Y[3 + i] = y_estimate[i];
        if (flag_serializer)
            out_str << "Output vector: " << Y << "\n\n";

        if (flag_serializer)
            out_str << "Ininial Forward(0) sweep...\n\n";
        // Declare a tape function and stop the tape recording.
        cl::tape_function<double> f(X, Y);
        std::clock_t stop_time = std::clock();
        out_str << "Tape memory (bytes): " << f.Memory() << std::endl;
        out_str << "Tape creation took (ms): " << (stop_time - start_time) / (double)(CLOCKS_PER_SEC)* 1000 << '\n';

        // Derivative calculation time.
        std::clock_t calc_time = 0;

        // Derivatives with respect to a.
        std::vector<double> dx = { 1, 0, 0, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        std::vector<double> forw;
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to b.
        dx = { 0, 1, 0, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to c.
        dx = { 0, 0, 1, 0 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        // Derivatives with respect to d.
        dx = { 0, 0, 0, 1 };
        if (flag_serializer)
            out_str << "Forward(1, dx) sweep for dx = " << dx << "..." << std::endl;
        start_time = std::clock();
        if (flag_serializer)
            forw = f.Forward(1, dx, out_str);
        else
            forw = f.Forward(1, dx);
        stop_time = std::clock();
        calc_time += stop_time - start_time;

        out_str << "Forward calculation took (ms): " << calc_time / (double)(CLOCKS_PER_SEC)* 1000 << '\n';
        out_str << std::endl;
    }


    inline void quadratic_regression_examples()
    {
        std::ofstream of("quadratic_regression_output.txt");
        CppAD::tape_serializer<cl::InnerArray> serializer(of);
        serializer.precision(3);

        // Input data parameters.
        quadratic_initial_data data;

        quadratic_regression_with_params_optimized_example(data, serializer);
        quadratic_regression_with_params_nonoptimized_example(data, serializer);
    }
}

#endif // cl_tape_examples_impl_quadratic_regression_examples_hpp
