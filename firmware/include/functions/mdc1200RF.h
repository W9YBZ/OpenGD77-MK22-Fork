/* Experimental GD77 board adapter; repository license.txt applies. */
#ifndef GD77_MDC_RF_H
#define GD77_MDC_RF_H
#include <stdbool.h>
#include <stdint.h>
#ifndef GD77_EXPERIMENTAL_MDC_RF
#define GD77_EXPERIMENTAL_MDC_RF 0
#endif
#if GD77_EXPERIMENTAL_MDC_RF && !defined(PLATFORM_GD77)
#error "Experimental MDC RF is GD77 only"
#endif
#if GD77_EXPERIMENTAL_MDC_RF
typedef enum { MDC_RF_IDLE, MDC_RF_RUNNING, MDC_RF_DONE, MDC_RF_FAULT } mdcRFStatus_t;
typedef struct {
 uint32_t busHz, maxLateTicks, maxWriteTicks, writes, reason;
 uint16_t unitId, symbols;
} mdcRFDiagnostics_t;
/* Caller has established legal FM TX, tone1=1800Hz, MIC disconnected and
 * owns a register snapshot. This backend never enables the PA or changes power. */
bool mdcRFStart(uint16_t id);
mdcRFStatus_t mdcRFStatus(void);
void mdcRFStop(void);
mdcRFDiagnostics_t mdcRFDiagnostics(void);
#endif
#endif
