#include <math.h>

uint32_t Ue(unsigned char *pBuff, uint32_t nLen, uint32_t &nStartBit)
{
    //计算0bit的个数
    uint32_t nZeroNum = 0;
    while (nStartBit < nLen * 8)
    {
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:按位与，%取余
        {
            break;
        }
        nZeroNum++;
        nStartBit++;
    }
    nStartBit++;


    //计算结果
    int dwRet = 0;
    for (uint32_t i = 0; i < nZeroNum; i++)
    {
        dwRet <<= 1;
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return (1 << nZeroNum) - 1 + dwRet;
}


int Se(unsigned char *pBuff, uint32_t nLen, uint32_t &nStartBit)
{


    int UeVal = Ue(pBuff, nLen, nStartBit);
    double k = UeVal;
    int nValue = (int)ceil(k / 2);//ceil函数：ceil函数的作用是求不小于给定实数的最小整数。ceil(2)=ceil(1.2)=cei(1.5)=2.00
    if (UeVal % 2 == 0)
        nValue = -nValue;
    return nValue;


}


int u(uint32_t BitCount, unsigned char * buf, uint32_t &nStartBit)
{
    int dwRet = 0;
    for (uint32_t i = 0; i < BitCount; i++)
    {
        dwRet <<= 1;
        if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return dwRet;
}


int h264_decode_seq_parameter_set(unsigned char * buf, uint32_t nLen, unsigned int &Width, unsigned int &Height)
{
    uint32_t StartBit = 0;
#if 0 /* set but not used */
    int log2_max_pic_order_cnt_lsb_minus4;
#endif
    #if 0 /* unused variables */
    int forbidden_zero_bit = u(1, buf, StartBit);
    int nal_ref_idc = u(2, buf, StartBit);
    #endif
    int nal_unit_type = u(5, buf, StartBit);
    if (nal_unit_type == 7)
    {
        int profile_idc = u(8, buf, StartBit);
        #if 0 /* unused variables */
        int constraint_set0_flag = u(1, buf, StartBit);//(buf[1] & 0x80)>>7;
        int constraint_set1_flag = u(1, buf, StartBit);//(buf[1] & 0x40)>>6;
        int constraint_set2_flag = u(1, buf, StartBit);//(buf[1] & 0x20)>>5;
        int constraint_set3_flag = u(1, buf, StartBit);//(buf[1] & 0x10)>>4;
        int reserved_zero_4bits = u(4, buf, StartBit);
        int level_idc = u(8, buf, StartBit);

        int seq_parameter_set_id = Ue(buf, nLen, StartBit);
        #endif

        if (profile_idc == 100 || profile_idc == 110 ||
            profile_idc == 122 || profile_idc == 144)
        {
            #if 0 /* unused variable */
            int chroma_format_idc = Ue(buf, nLen, StartBit);
            if (chroma_format_idc == 3)
                int residual_colour_transform_flag = u(1, buf, StartBit);
            #endif
            
            #if 0 /* unused variable */
            int bit_depth_luma_minus8 = Ue(buf, nLen, StartBit);
            int bit_depth_chroma_minus8 = Ue(buf, nLen, StartBit);
            int qpprime_y_zero_transform_bypass_flag = u(1, buf, StartBit);
            #endif
            int seq_scaling_matrix_present_flag = u(1, buf, StartBit);
            #if 0 /* variable set but not used */
            int seq_scaling_list_present_flag[8];
            #endif
            if (seq_scaling_matrix_present_flag)
            {
                for (int i = 0; i < 8; i++) {
                    /* variable set but not used */
                    /* seq_scaling_list_present_flag[i] =*/(void)u(1, buf, StartBit);
                }
            }
        }
        #if 0 /* unused variable */
        int log2_max_frame_num_minus4 = Ue(buf, nLen, StartBit);
        #endif
        int pic_order_cnt_type = Ue(buf, nLen, StartBit);
        if (pic_order_cnt_type == 0)
            /* set but not used */
            /* log2_max_pic_order_cnt_lsb_minus4 =*/(void)Ue(buf, nLen, StartBit);
        else if (pic_order_cnt_type == 1)
        {
            #if 0 /* unused variables */
            int delta_pic_order_always_zero_flag = u(1, buf, StartBit);
            int offset_for_non_ref_pic = Se(buf, nLen, StartBit);
            int offset_for_top_to_bottom_field = Se(buf, nLen, StartBit);
            #endif
            int num_ref_frames_in_pic_order_cnt_cycle = Ue(buf, nLen, StartBit);

            int *offset_for_ref_frame = new int[num_ref_frames_in_pic_order_cnt_cycle];
            for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
                offset_for_ref_frame[i] = Se(buf, nLen, StartBit);
            delete[] offset_for_ref_frame;
        }
        /* set but not used */
        /* int num_ref_frames = */(void)Ue(buf, nLen, StartBit);
        #if 0 /* unssed variable */
        int gaps_in_frame_num_value_allowed_flag = u(1, buf, StartBit);
        #endif
        int pic_width_in_mbs_minus1 = Ue(buf, nLen, StartBit);
        int pic_height_in_map_units_minus1 = Ue(buf, nLen, StartBit);

        Width = (pic_width_in_mbs_minus1 + 1) * 16;
        Height = (pic_height_in_map_units_minus1 + 1) * 16;

        return 1;
    }
    else
        return 0;
}
