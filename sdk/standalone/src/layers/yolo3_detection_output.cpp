//-----------------------------------------------------------------------------
// The confidential and proprietary information contained in this file may
// only be used by a person authorised under and to the extent permitted
// by a subsisting licensing agreement from EMBEDEEP Corporation.
//
//    (C) COPYRIGHT 2018-2022  EMBEDEEP Corporation.
//        ALL RIGHTS RESERVED
//
// This entire notice must be reproduced on all copies of this file
// and copies of this file may only be made by a person if such person is
// permitted to do so under the terms of a subsisting license agreement
// from EMBEDEEP Corporation.
//
//      Main version        : V0.0.1
//
//      Revision            : 2022-10-08
//
//-----------------------------------------------------------------------------

//#include <vector>
//#include <algorithm>
#include <math.h>
#include "../simplestl.h"

#include "../eeptpu/nmat.h"

using namespace std;

static int num_class;
static int num_box;
static float confidence_threshold;
static float nms_threshold;
static vector<float> biases;
static vector<float> mask;
static vector<float> anchors_scale;
static int mask_group_num;


static bool b_inited = false;

/*
layer {
  name: "detection_out"
  type: "Yolov3DetectionOutput"
  bottom: "conv19"
  bottom: "conv20"
  top: "detection_out"
  include {
    phase: TEST
  }
  yolov3_detection_output_param {
    num_classes: 20
    confidence_threshold: 0.25
    nms_threshold: 0.449999988079
    biases: 10.0
    biases: 14.0
    biases: 23.0
    biases: 27.0
    biases: 37.0
    biases: 58.0
    biases: 81.0
    biases: 82.0
    biases: 135.0
    biases: 169.0
    biases: 344.0
    biases: 319.0
    anchors_scale: 32
    anchors_scale: 16
    mask_group_num: 2
    mask: 3
    mask: 4
    mask: 5
    mask: 0
    mask: 1
    mask: 2
  }
}
*/

int yolo3_detection_output_init_params()
{
    num_class = 80;      // 对应yolo层classes
    num_box = 3;         // 单个yolo层里mask数值的个数
    confidence_threshold = 0.25;
    nms_threshold = 0.5;   // 对应yolo层的nms_thresh， 没有的话设置为0.45

    biases.clear();
    mask.clear();
    anchors_scale.clear();

    // 10,14,  23,27,  37,58,  81,82,  135,169,  344,319
    biases.push_back(10.000000);     // 对应yolo层的anchors，每个yolo层该参数都一样
    biases.push_back(14.000000);
    biases.push_back(23.000000);
    biases.push_back(27.000000);
    biases.push_back(37.000000);
    biases.push_back(58.000000);
    biases.push_back(81.000000);
    biases.push_back(82.000000);
    biases.push_back(135.000000);
    biases.push_back(169.000000);
    biases.push_back(344.000000);
    biases.push_back(319.000000);

    mask.push_back(3.000000);   // 对应yolo层mask。每个yolo层不一样。要按yolo层顺序来添加
    mask.push_back(4.000000);
    mask.push_back(5.000000);
    mask.push_back(1.000000);
    mask.push_back(2.000000);
    mask.push_back(3.000000);

    // 几个yolo层，就有几个anchors_scale
    // 计算方式： input_w * scale_x_y / yolo_w
    // input_w为输入维度416
    // scale_x_y为yolo层里的参数1.05
    // yolo_w为yolo层的输入维度。可以用可视化软件Netron查看yolo层输入，每个yolo层输入维度不一样。
    // 这里yolo4tiny的yolo层输入为13和26
    // 416*1.05/13=33.6
    // 416*1.05/26=16.8
    anchors_scale.push_back(33.600000);
    anchors_scale.push_back(16.800000);

    return 0;
}

struct BBoxRect
{
    float xmin;
    float ymin;
    float xmax;
    float ymax;
    int label;
};

static inline float intersection_area(const BBoxRect& a, const BBoxRect& b)
{
    if (a.xmin > b.xmax || a.xmax < b.xmin || a.ymin > b.ymax || a.ymax < b.ymin)
    {
        // no intersection
        return 0.f;
    }

    float inter_width = std::min(a.xmax, b.xmax) - std::max(a.xmin, b.xmin);
    float inter_height = std::min(a.ymax, b.ymax) - std::max(a.ymin, b.ymin);

    return inter_width * inter_height;
}

template <typename T>
static void qsort_descent_inplace(std::vector<T>& datas, std::vector<float>& scores, int left, int right)
{
    int i = left;
    int j = right;
    float p = scores[(left + right) / 2];

    while (i <= j)
    {
        while (scores[i] > p)
            i++;

        while (scores[j] < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(datas[i], datas[j]);
            std::swap(scores[i], scores[j]);

            i++;
            j--;
        }
    }

    if (left < j)
        qsort_descent_inplace(datas, scores, left, j);

    if (i < right)
        qsort_descent_inplace(datas, scores, i, right);
}

template <typename T>
static void qsort_descent_inplace(std::vector<T>& datas, std::vector<float>& scores)
{
    if (datas.empty() || scores.empty())
        return;

    qsort_descent_inplace(datas, scores, 0, scores.size() - 1);
}

static void nms_sorted_bboxes(const std::vector<BBoxRect>& bboxes, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();

    const int n = bboxes.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        const BBoxRect& r = bboxes[i];

        float width = r.xmax - r.xmin;
        float height = r.ymax - r.ymin;

        areas[i] = width * height;
    }

    for (int i = 0; i < n; i++)
    {
        const BBoxRect& a = bboxes[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const BBoxRect& b = bboxes[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
//             float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

static inline float sigmoid(float x)
{
    return 1.f / (1.f + exp(-x));
}

#include <stdio.h>
int yolo3_detection_output_forward(const std::vector<ncnn::Mat>& bottom_blobs, std::vector<ncnn::Mat>& top_blobs)
{
    if (b_inited == false) { yolo3_detection_output_init_params(); b_inited = true; }

	// gather all box
	std::vector<BBoxRect> all_bbox_rects;
	std::vector<float> all_bbox_scores;

	for (size_t b = 0; b < bottom_blobs.size(); b++)
	{
		std::vector< std::vector<BBoxRect> > all_box_bbox_rects;
		std::vector< std::vector<float> > all_box_bbox_scores;
		all_box_bbox_rects.resize(num_box);
		all_box_bbox_scores.resize(num_box);
		const ncnn::Mat& bottom_top_blobs = bottom_blobs[b];

		int w = bottom_top_blobs.w;
		int h = bottom_top_blobs.h;
		int channels = bottom_top_blobs.c;
		const int channels_per_box = channels / num_box;

		// anchor coord + box score + num_class
		if (channels_per_box != 4 + 1 + num_class)
	    {
			return -1;
		}
		int mask_offset = b * num_box;
		int net_w = (int)(anchors_scale[b] * w);
		int net_h = (int)(anchors_scale[b] * h);

#pragma omp parallel for num_threads(4)
		for (int pp = 0; pp < num_box; pp++)
		{
			int p = pp * channels_per_box;
			int biases_index = mask[pp + mask_offset];
			const float bias_w = biases[biases_index * 2];
			const float bias_h = biases[biases_index * 2 + 1];
			const float* xptr = bottom_top_blobs.channel(p);
			const float* yptr = bottom_top_blobs.channel(p + 1);
			const float* wptr = bottom_top_blobs.channel(p + 2);
			const float* hptr = bottom_top_blobs.channel(p + 3);

			const float* box_score_ptr = bottom_top_blobs.channel(p + 4);

			// softmax class scores
			ncnn::Mat scores = bottom_top_blobs.channel_range(p + 5, num_class);
			//softmax->forward_inplace(scores, opt);

			for (int i = 0; i < h; i++)
			{
				for (int j = 0; j < w; j++)
				{
					// region box
					float bbox_cx = (j + sigmoid(xptr[0])) / w;
					float bbox_cy = (i + sigmoid(yptr[0])) / h;
					float bbox_w = exp(wptr[0]) * bias_w / net_w;
					float bbox_h = exp(hptr[0]) * bias_h / net_h;

					float bbox_xmin = bbox_cx - bbox_w * 0.5f;
					float bbox_ymin = bbox_cy - bbox_h * 0.5f;
					float bbox_xmax = bbox_cx + bbox_w * 0.5f;
					float bbox_ymax = bbox_cy + bbox_h * 0.5f;

					// box score
					float box_score = sigmoid(box_score_ptr[0]);

					// find class index with max class score
					int class_index = 0;
					float class_score = 0.f;
					for (int q = 0; q < num_class; q++)
					{
						float score = sigmoid(scores.channel(q).row(i)[j]);
						if (score > class_score)
						{
							class_index = q;
							class_score = score;
						}
					}

					float confidence = box_score * class_score;
					if (confidence >= confidence_threshold)
					{
						BBoxRect c = { bbox_xmin, bbox_ymin, bbox_xmax, bbox_ymax, class_index };
						all_box_bbox_rects[pp].push_back(c);
						all_box_bbox_scores[pp].push_back(confidence);
					}

					xptr++;
					yptr++;
					wptr++;
					hptr++;

					box_score_ptr++;
				}
			}
		}

		for (int i = 0; i < num_box; i++)
		{
			const std::vector<BBoxRect>& box_bbox_rects = all_box_bbox_rects[i];
			const std::vector<float>& box_bbox_scores = all_box_bbox_scores[i];

			all_bbox_rects.insert(all_bbox_rects.end(), box_bbox_rects.begin(), box_bbox_rects.end());
			all_bbox_scores.insert(all_bbox_scores.end(), box_bbox_scores.begin(), box_bbox_scores.end());
		}

	}

    // global sort inplace
    qsort_descent_inplace(all_bbox_rects, all_bbox_scores);

    // apply nms
    std::vector<int> picked;
    nms_sorted_bboxes(all_bbox_rects, picked, nms_threshold);

    // select
    std::vector<BBoxRect> bbox_rects;
    std::vector<float> bbox_scores;

    for (int i = 0; i < (int)picked.size(); i++)
    {
        int z = picked[i];
        bbox_rects.push_back(all_bbox_rects[z]);
        bbox_scores.push_back(all_bbox_scores[z]);
    }

    // fill result
    int num_detected = bbox_rects.size();
	ncnn::Mat top_blob;
	top_blob.create(6, num_detected, 1, 4u);
    if (top_blob.empty())
        return -100;

    for (int i = 0; i < num_detected; i++)
    {
        const BBoxRect& r = bbox_rects[i];
        float score = bbox_scores[i];
        float* outptr = top_blob.row(i);

        outptr[0] = r.label + 1;// +1 for prepend background class
        outptr[1] = score;
        outptr[2] = r.xmin;
        outptr[3] = r.ymin;
        outptr[4] = r.xmax;
        outptr[5] = r.ymax;
    }
    top_blobs.push_back(top_blob);

    return 0;
}




