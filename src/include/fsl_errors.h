/**************************************************************************//**
            Copyright 2013 Freescale Semiconductor, Inc.

 @File          fsl_errors.h

 @Description   Error definitions.
*//***************************************************************************/

#ifndef __FSL_ERRORS_H
#define __FSL_ERRORS_H

#include <errno.h>
#include "inc/errors.h"



/**************************************************************************//**
 @Group         Errors

 @Description   Error return defines.

 @{
*//***************************************************************************/

/*
 * Even though this file includes <errno.h>, developers are guided to use only
 * the following subset, for reasons of consistency and clarity.
 */

#ifndef EPERM
#define EPERM		1	/*!< Permission denied, no privileges */
#endif

#ifndef EIO
#define EIO		5	/*!< Input/output error */
#endif

#ifndef ENOMEM
#define ENOMEM		12	/*!< Out of memory */
#endif

#ifndef EACCES
#define EACCES		13	/*!< Illegal access, or invalid device state */
#endif

#ifndef EFAULT
#define EFAULT		14	/*!< Bad address detected */
#endif

#ifndef EBUSY
#define EBUSY		16	/*!< Device or resource is busy */
#endif

#ifndef EEXIST
#define EEXIST		17	/*!< Resource already exists */
#endif

#ifndef ENODEV
#define ENODEV		19	/*!< No such device, or device not configured */
#endif

#ifndef EINVAL
#define EINVAL		22	/*!< Invalid argument, or conflicting arguments */
#endif

#ifndef ENOSPC
#define ENOSPC		28	/*!< No space left (resource is full) */
#endif

#ifndef ENOTSUP
#define ENOTSUP		95	/*!< Operation not supported */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT	110	/*!< Operation timed out */
#endif

#ifndef ENAVAIL
#define ENAVAIL		119	/*!< Resource not available, or not found */
#endif

#undef ENOERR			/* Prevent usage */

#endif /* __FSL_ERRORS_H */