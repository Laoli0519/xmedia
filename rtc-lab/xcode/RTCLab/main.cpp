//
//  main.cpp
//  RTCLab
//
//  Created by simon on 2018/7/24.
//  Copyright © 2018年 easemob. All rights reserved.
//

#include <iostream>

extern "C"{
    int lab_vp8_main(int argc, char* argv[]);
}
int main(int argc, char * argv[]) {
    int ret = 0;
    ret = lab_vp8_main(argc, argv);
    return 0;
}
