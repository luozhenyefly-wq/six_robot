/*
	腿部运动逆解算
	定义一条腿上三个关节髋关节J1,大腿关节J2,小腿关节J3
	J1->J2  L1    1 转轴到 J2 的这一段，方向固定 0°，即从转轴水平伸出去
	J2->J3  L2    大腿
	J3      L3    小腿
*/
#include "leg_angle.h"

#define L1_ANGLE    (0.0f)

LegMount LEG1,LEG2,LEG3,LEG4,LEG5,LEG6;//六条腿

void LEG_Init(void)
{
	LEG1.mx=10.3f;//J1 转轴在机体坐标下的位置
    LEG1.my=6.15f;
    LEG1.mz=2.0f;
    LEG1.psi=45.0f;//安装方位角（度）
    LEG1.L1=4.3f;//髋段
    LEG1.L2=7.4f;//大腿
    LEG1.L3=12.3f;//小腿
	
	LEG2.mx=0.0f;//J1 转轴在机体坐标下的位置
    LEG2.my=7.7f;
    LEG2.mz=2.0f;
    LEG2.psi=0.0f;//安装方位角（度）
    LEG2.L1=4.3f;//髋段
    LEG2.L2=7.4f;//大腿
    LEG2.L3=12.3f;//小腿

	LEG3.mx=-10.3f;//J1 转轴在机体坐标下的位置
    LEG3.my=6.15f;
    LEG3.mz=2.0f;
    LEG3.psi=-45.0f;//安装方位角（度）
    LEG3.L1=4.3f;//髋段
    LEG3.L2=7.4f;//大腿
    LEG3.L3=12.3f;//小腿
	
	LEG4.mx=10.3f;//J1 转轴在机体坐标下的位置
    LEG4.my=-6.15f;
    LEG4.mz=2.0f;
    LEG4.psi=-45.0f;//安装方位角（度）
    LEG4.L1=4.3f;//髋段
    LEG4.L2=7.4f;//大腿
    LEG4.L3=12.3f;//小腿
	
	LEG5.mx=0.0f;//J1 转轴在机体坐标下的位置
    LEG5.my=-7.7f;
    LEG5.mz=2.0f;
    LEG5.psi=0.0f;//安装方位角（度）
    LEG5.L1=4.3f;//髋段
    LEG5.L2=7.4f;//大腿
    LEG5.L3=12.3f;//小腿

	LEG6.mx=-10.3f;//J1 转轴在机体坐标下的位置
    LEG6.my=-6.15f;
    LEG6.mz=2.0f;
    LEG6.psi=45.0f;//安装方位角（度）
    LEG6.L1=4.3f;//髋段
    LEG6.L2=7.4f;//大腿
    LEG6.L3=12.3f;//小腿
}
	

// 弧度 → 角度
float rad_to_deg(float rad)
{
    return rad * 180.0f / PI;
}


// 角度 → 弧度
float deg_to_rad(float deg)
{
    return deg * PI / 180.0f;
}


/*
  单腿逆运动学（腿坐标系）
 
  L3 放开：髋 J2 和膝 J3 两个关节一起定位，所以有两个解
 
  solution：1 和 -1 对应两种姿态（膝朝前 / 膝朝后）
 
  theta1：J1 转动角（左腿 atan2(x, y)，右腿 atan2(-x, -y)）
  theta2：J2 转动角，L2 的绝对方向角
  theta3：J3 的相对转动角（L3 相对 L2 的转角，带正负）
  *_real：对应舵机的实际输出角度
 */
LegAngle Leg_IK(float x, float y, float z,
                float L1, float L2, float L3,
                int solution, int left_or_right)
{
    LegAngle angle;

    float r_max = L2 + L3;            //L2,L3能伸距离的最大值
    float r_min = fabsf(L2 - L3);     //L2,L3能伸距离的最小值，超出范围均无法到达

    // 第一步：J1 底座 yaw（左右腿镜像）
    float theta1 = 0;

    if(left_or_right == LEFT_LEG)
    {
        theta1 = atan2f(x, y);
    }
    else if(left_or_right == RIGHT_LEG)
    {
        theta1 = atan2f(-x, -y);
    }

    float r = sqrtf(x * x + y * y);

    // 第二步：J2 的位置
    float phi1 = deg_to_rad(L1_ANGLE);

    float y2 = L1 * cosf(phi1);

    float z2 = L1 * sinf(phi1);

    // 第三步：计算 J2 到末端的距离
    float dy = r - y2;

    float dz = z - z2;

    float r2 = dy * dy + dz * dz;

    float rr = sqrtf(r2);

    if(rr > r_max || rr < r_min)
    {
        //无法到达：三个舵机都给中位
        angle.theta1_real = 135.0f;
        angle.theta2_real = 135.0f;
        angle.theta3_real = 180.0f;

        return angle;
    }

    // 第四步：计算 cos(theta3)
    float cos_theta3;

    cos_theta3 = (r2 - L2 * L2 - L3 * L3) / (2.0f * L2 * L3);

    // 防止浮点误差
    if(cos_theta3 > 1.0f)
        cos_theta3 = 1.0f;

    if(cos_theta3 < -1.0f)
        cos_theta3 = -1.0f;

    // 第五步：得到 theta3（正负号就是两种姿态）
    float theta3;

    if(solution == 1)
    {
        theta3 = acosf(cos_theta3);
    }
    else
    {
        theta3 = -acosf(cos_theta3);
    }

    // 第六步：计算 theta2（L2 的绝对方向角）
    float theta2;

    theta2 = atan2f(dz, dy) - atan2f(L3 * sinf(theta3), L2 + L3 * cosf(theta3));

    // 第七步：弧度 → 角度
    angle.theta1 = rad_to_deg(theta1);

    angle.theta2 = rad_to_deg(theta2);

    angle.theta3 = rad_to_deg(theta3);

    // 舵机实际输出角度
    angle.theta1_real = 135.0f - angle.theta1;

    angle.theta2_real = angle.theta2 + 135.0f;

    angle.theta3_real = angle.theta3 + 180.0f;

    return angle;
}


/*
  机体坐标 → 单腿坐标
 
  【角度约定】（和 Leg_IK 里 theta1 的算法一致，别改）
    方位角以机体的【+y 轴】为 0°，往【+x 轴】方向为正：
 
            前 +x
              ↑
      +y ←────┼────→ -y
      左      │      右
              ↓
            后 -x
 
    正方向：左(+y) → 前(+x) → 右(-y) → 后(-x)   （俯视看是顺时针）
 
    psi 的物理含义：这条腿 J1 舵机在中位（theta1 = 0）时，
                    髋段 L1 伸出去的那个方向，在机体里的方位角
 
  变换：先减掉基座位置 (mx,my,mz)，再把方位角转掉（azi_leg = azi_body - psi）
 */
void Body_To_Leg(LegMount *leg, float X, float Y, float Z,
                 float *x, float *y, float *z)
{
    float dx = X - leg->mx;

    float dy = Y - leg->my;

    float r = sqrtf(dx * dx + dy * dy);

    float azi_body = atan2f(dx, dy);

    float azi_leg = azi_body - deg_to_rad(leg->psi);

    *x = r * sinf(azi_leg);

    *y = r * cosf(azi_leg);

    *z = Z - leg->mz;
}


/*
  给机体坐标，直接出这条腿的关节角
 */
LegAngle Leg_Solve(LegMount *leg, float X, float Y, float Z,
                   int solution, int left_or_right)
{
    float x, y, z;

    Body_To_Leg(leg, X, Y, Z, &x, &y, &z);

    return Leg_IK(x, y, z, leg->L1, leg->L2, leg->L3, solution, left_or_right);
}
