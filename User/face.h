#ifndef __FACE_H
#define __FACE_H

#include "main.h"

/* 表情种类 */
#define FACE_NORMAL     0u      /* 平常 */
#define FACE_HAPPY      1u      /* 开心 */
#define FACE_SAD        2u      /* 难过 */
#define FACE_ANGRY      3u      /* 生气 */
#define FACE_SURPRISE   4u      /* 惊讶 */
#define FACE_SLEEP      5u      /* 困了 */
#define FACE_SHAZI      6u      /* 傻子 */

void Face_Init(void);
void Face_Update(void);                 /* 由任务周期调用，内部做节流 */
void Face_SetEmotion(uint8_t emotion);

#endif
