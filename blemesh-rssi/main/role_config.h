#pragma once

// 하나의 펌웨어로 역할 스위치 (빌드 타임 혹은 런타임)
#define ROLE_CHILD   0
#define ROLE_ANCHOR  1
#define ROLE_PROXY   2

#ifndef NODE_ROLE
#define NODE_ROLE ROLE_ANCHOR   // 기본은 Anchor
#endif