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

#include "../simplestl.h"
#include "../eeptpu/nmat.h"

static int get_topk(const std::vector<float>& cls_scores, unsigned int topk, std::vector< std::pair<float, int> >& top_list)
{
    if (cls_scores.size() < topk) topk = cls_scores.size();

    int size = cls_scores.size();
    std::vector< std::pair<float, int> > vec;
    vec.resize(size);
    for (int i=0; i<size; i++)
    {
        vec[i] = std::make_pair(cls_scores[i], i);
    }

    std::partial_sort(vec.begin(), vec.begin() + topk, vec.end(),
                      std::greater< std::pair<float, int> >());

    top_list.resize(topk);
    for (unsigned int i=0; i<topk; i++)
    {
        top_list[i].first = vec[i].first;
        top_list[i].second = vec[i].second;
    }
    vec.clear();

    return 0;
}

int get_topk(ncnn::Mat& result , int topk, std::vector< std::pair<float, int> >& top_list)
{
    std::vector<float> cls_scores;
    cls_scores.resize(result.c*result.h*result.w);

    int i = 0;
    for (int c = 0; c < result.c; c++)
    {
        float *ptrf = (float*)result.channel(c).data;
        for (int hw = 0; hw < result.h * result.w; hw++)
        {
            cls_scores[i++] = *ptrf++;
        }
    }

    get_topk(cls_scores, topk, top_list);
    cls_scores.clear();
    return 0;
}


