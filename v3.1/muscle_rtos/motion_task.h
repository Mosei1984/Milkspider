#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Motion task entry point.
 * Consumes PosePacket31 from queue, clamps values, interpolates, outputs to PCA9685.
 */
void motion_task_entry(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // MOTION_TASK_H
