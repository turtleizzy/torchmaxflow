#include <torch/extension.h>
#include <vector>
// #include <iostream>
#include "maxflow-v3.04.src/graph.h"

// void print_shape(torch::Tensor data)
// {
//     auto num_dims = data.dim();
//     std::cout << "Shape: (";
//     for (int dim = 0; dim < num_dims; dim++)
//     {
//         std::cout << data.size(dim);
//         if (dim != num_dims - 1)
//         {
//             std::cout << ", ";
//         }
//         else
//         {
//             std::cout << ")" << std::endl;
//         }
//     }
// }

// float l1distance(const float &in1, const float &in2)
// {
//     return std::abs(in1 - in2);
// }

// float l1distance(const float *in1, const float *in2, int size)
// {
//     float ret_sum = 0.0;
//     for (int c_i = 0; c_i < size; c_i++)
//     {
//         ret_sum += abs(in1[c_i] - in2[c_i]);
//     }
//     return ret_sum;
// }

float l2distance(const float &in1, const float &in2)
{
    return std::abs(in1 - in2);
}

float l2distance(const float *in1, const float *in2, int size)
{
    float ret_sum = 0.0;
    for (int c_i = 0; c_i < size; c_i++)
    {
        ret_sum += (in1[c_i] - in2[c_i]) * (in1[c_i] - in2[c_i]);
    }
    return std::sqrt(ret_sum);
}

torch::Tensor maxflow2d_cpu(const torch::Tensor &image, const torch::Tensor &prob, const float &lambda, const float &sigma)
{
    // get input dimensions
    const int channel = image.size(1);
    const int height = image.size(2);
    const int width = image.size(3);

    const int Xoff[2] = {-1, 0};
    const int Yoff[2] = {0, -1};

    // prepare output
    torch::Tensor label = torch::zeros_like(image);

    // get data accessors
    auto label_ptr = label.accessor<float, 4>();
    auto image_ptr = image.accessor<float, 4>();
    auto prob_ptr = prob.accessor<float, 4>();

    // prepare graph
    typedef Graph<float, float, float> GraphType;
    // initialise with graph(num of nodes, num of edges)
    GraphType *g = new GraphType(height * width, 2 * height * width);
    g->add_node(height * width);

    // define max weight value
    float max_weight = -100000;

    for (int h = 0; h < height; h++)
    {
        for (int w = 0; w < width; w++)
        {
            float pval;
            float pval_v[channel];
            if (channel == 1)
            {
                pval = image_ptr[0][0][h][w];
            }
            else
            {
                for (int c_i = 0; c_i < channel; c_i++)
                {
                    pval_v[c_i] = image_ptr[0][c_i][h][w];
                }
            }
            float l2dis, n_weight;
            int pIndex = h * width + w;
            int qIndex;

            for (int i = 0; i < 2; i++)
            {
                const int hn = h + Xoff[i];
                const int wn = w + Yoff[i];
                if (hn < 0 || wn < 0)
                    continue;

                float qval;
                float qval_v[channel];
                if (channel == 1)
                {
                    qval = image_ptr[0][0][hn][wn];
                    l2dis = l2distance(pval, qval);
                }
                else
                {
                    for (int c_i = 0; c_i < channel; c_i++)
                    {
                        qval_v[c_i] = image_ptr[0][c_i][hn][wn];
                    }
                    l2dis = l2distance(pval_v, qval_v, channel);
                }
                n_weight = lambda * exp(-(l2dis * l2dis) / (2 * sigma * sigma));
                qIndex = hn * width + wn;
                g->add_edge(qIndex, pIndex, n_weight, n_weight);

                if (n_weight > max_weight)
                {
                    max_weight = n_weight;
                }
            }
        }
    }

    max_weight = 1000 * max_weight;
    for (int h = 0; h < height; h++)
    {
        for (int w = 0; w < width; w++)
        {
            float s_weight, t_weight;

            float prob_bg, prob_fg;
            // avoid log(0)
            prob_bg = std::max(prob_ptr[0][0][h][w], std::numeric_limits<float>::epsilon());
            prob_fg = std::max(prob_ptr[0][1][h][w], std::numeric_limits<float>::epsilon());
            s_weight = -log(prob_bg);
            t_weight = -log(prob_fg);

            int pIndex = h * width + w;
            g->add_tweights(pIndex, s_weight, t_weight);
        }
    }
    double flow = g->maxflow();
    // std::cout << "max flow: " << flow << std::endl;

    int idx = 0;
    for (int h = 0; h < height; h++)
    {
        for (int w = 0; w < width; w++)
        {
            label_ptr[0][0][h][w] = 1 - g->what_segment(idx);
            idx++;
        }
    }
    delete g;
    return label;
}

torch::Tensor maxflow3d_cpu(const torch::Tensor &image, const torch::Tensor &prob, const float &lambda, const float &sigma)
{
    // get input dimensions
    const int channel = image.size(1);
    const int depth = image.size(2);
    const int height = image.size(3);
    const int width = image.size(4);

    const int Xoff[3] = {-1, 0, 0};
    const int Yoff[3] = {0, -1, 0};
    const int Zoff[3] = {0, 0, -1};

    // prepare output
    torch::Tensor label = torch::zeros_like(image);

    // get data accessors
    auto label_ptr = label.accessor<float, 5>();
    auto image_ptr = image.accessor<float, 5>();
    auto prob_ptr = prob.accessor<float, 5>();

    // prepare graph
    typedef Graph<float, float, float> GraphType;
    // initialise with graph(num of nodes, num of edges)
    GraphType *g = new GraphType(depth * height * width, 2 * depth * height * width);
    g->add_node(depth * height * width);

    // define max weight value
    float max_weight = -100000;

    for (int d = 0; d < depth; d++)
    {
        for (int h = 0; h < height; h++)
        {
            for (int w = 0; w < width; w++)
            {
                float pval;
                float pval_v[channel];
                if (channel == 1)
                {
                    pval = image_ptr[0][0][d][h][w];
                }
                else
                {
                    for (int c_i = 0; c_i < channel; c_i++)
                    {
                        pval_v[c_i] = image_ptr[0][c_i][d][h][w];
                    }
                }
                float l2dis, n_weight;
                int pIndex = d * height * width + h * width + w;
                int qIndex;

                for (int i = 0; i < 3; i++)
                {
                    const int dn = d + Xoff[i];
                    const int hn = h + Yoff[i];
                    const int wn = w + Zoff[i];
                    if (dn < 0 || hn < 0 || wn < 0)
                        continue;

                    float qval;
                    float qval_v[channel];
                    if (channel == 1)
                    {
                        qval = image_ptr[0][0][dn][hn][wn];
                        l2dis = l2distance(pval, qval);
                    }
                    else
                    {
                        for (int c_i = 0; c_i < channel; c_i++)
                        {
                            qval_v[c_i] = image_ptr[0][c_i][dn][hn][wn];
                        }
                        l2dis = l2distance(pval_v, qval_v, channel);
                    }
                    n_weight = lambda * exp(-l2dis * l2dis / (2 * sigma * sigma));
                    qIndex = dn * height * width + hn * width + wn;
                    g->add_edge(qIndex, pIndex, n_weight, n_weight);

                    if (n_weight > max_weight)
                    {
                        max_weight = n_weight;
                    }
                }
            }
        }
    }

    max_weight = 1000 * max_weight;
    for (int d = 0; d < depth; d++)
    {
        for (int h = 0; h < height; h++)
        {
            for (int w = 0; w < width; w++)
            {
                float s_weight, t_weight;

                float prob_bg, prob_fg;
                // avoid log(0)
                prob_bg = std::max(prob_ptr[0][0][d][h][w], std::numeric_limits<float>::epsilon());
                prob_fg = std::max(prob_ptr[0][1][d][h][w], std::numeric_limits<float>::epsilon());
                s_weight = -log(prob_bg);
                t_weight = -log(prob_fg);

                int pIndex = d * height * width + h * width + w;
                g->add_tweights(pIndex, s_weight, t_weight);
            }
        }
    }
    double flow = g->maxflow();
    // std::cout << "max flow: " << flow << std::endl;

    int idx = 0;
    for (int d = 0; d < depth; d++)
    {
        for (int h = 0; h < height; h++)
        {
            for (int w = 0; w < width; w++)
            {
                label_ptr[0][0][d][h][w] = 1 - g->what_segment(idx);
                idx++;
            }
        }
    }
    delete g;
    return label;
}

void add_interactive_seeds(torch::Tensor &prob, const torch::Tensor &seed, const int &num_dims)
{
    // implements Equation 7 from:
    //  Wang, Guotai, et al. 
    //  "Interactive medical image segmentation using deep learning with image-specific fine tuning." 
    //  IEEE TMI (2018).
    
    const int channel = prob.size(1);
    if(num_dims == 4) // 2D
    {
        const int height = prob.size(2);
        const int width = prob.size(3);
        
        auto prob_ptr = prob.accessor<float, 4>();
        auto seed_ptr = seed.accessor<float, 4>();

        for (int h=0; h < height; h++)
        {
            for (int w=0; w<width; w++)
            {
                if(seed_ptr[0][0][h][w] > 0)
                {
                    prob_ptr[0][0][h][w] = 1.0;
                    prob_ptr[0][1][h][w] = 0.0;

                }
                else if (seed_ptr[0][1][h][w] > 0)
                {
                    prob_ptr[0][0][h][w] = 0.0;
                    prob_ptr[0][1][h][w] = 1.0;

                }
            }
        }

    }
    else if(num_dims == 5) // 3D
    {
        const int depth = prob.size(2);
        const int height = prob.size(3);
        const int width = prob.size(4);
        
        auto prob_ptr = prob.accessor<float, 5>();
        auto seed_ptr = seed.accessor<float, 5>();
        
        for (int d=0; d < depth; d++)
        {
            for (int h=0; h < height; h++)
            {
                for (int w=0; w<width; w++)
                {
                    if(seed_ptr[0][0][d][h][w] > 0)
                    {
                        prob_ptr[0][0][d][h][w] = 1.0;
                        prob_ptr[0][1][d][h][w] = 0.0;

                    }
                    else if (seed_ptr[0][1][d][h][w] > 0)
                    {
                        prob_ptr[0][0][d][h][w] = 0.0;
                        prob_ptr[0][1][d][h][w] = 1.0;

                    }
                }
            }
        }
    }
}