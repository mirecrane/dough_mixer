#ifndef __HTTP_H__
#define __HTTP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 HTTP 服务器
 *
 * 在端口 80 上启动 HTTP 服务器，提供和面机控制网页和 API：
 *   GET  /            — 返回控制面板网页
 *   POST /api/start   — 启动 motor_coordinated_control()
 *   GET  /api/status  — 返回电机状态 JSON
 *   POST /api/stop    — 调用 motor_emergency_stop()
 */
void http_server_start(void);

#ifdef __cplusplus
}
#endif

#endif
