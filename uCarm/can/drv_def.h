/*
****************************************************************************************************
*                                      Micrium, Inc.
*                                  949 Crestview Circle
*                                    Weston, FL 33327
*
*                                         uC/CAN
*
*                         (c) Copyright 2005 - 2009, Micrium, Inc.
*                                  All rights reserved.
*
*         All rights reserved.  Protected by international copyright laws.
*         Knowledge of the source code may not be used to write a similar
*         product.  This file may only be used in accordance with a license
*         and should not be redistributed in any way.
*
*
* Version       : 2.1.0
* File          : $file$
* Programmer(s) : EO
****************************************************************************************************
*/

#ifndef _DRV_DEF_H
#define _DRV_DEF_H


/**************************************************************************************************/
/*                         ACCESS TYPE DEFINITIONS                                                */
/**************************************************************************************************/


/*------------------------------------------------------------------------------------------------*/
/*! \brief                 EXLCUSIVE ACCESS
*
*            This define holds the value for mode coding bit 7: exclusive access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_EXCLUSIVE  (0 << 7)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 SHARED ACCESS
*
*            This define holds the value for mode coding bit 7: shared access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_SHARE      (1 << 7)


/**************************************************************************************************/
/*                         PERMISSION DEFINITIONS                                                 */
/**************************************************************************************************/


/*------------------------------------------------------------------------------------------------*/
/*! \brief                 EXECUTE PERMISSION
*
*            This define holds the value for mode coding bit 0: execute permission enabled
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_EXE        (1 << 0)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 WRITE PERMISSION
*
*            This define holds the value for mode coding bit 1: write permission enabled
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_WO         (1 << 1)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 READ PERMISSION
*
*            This define holds the value for mode coding bit 2: read permission enabled
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_RO         (1 << 2)


/**************************************************************************************************/
/*                         ACCESS MODE DEFINITIONS                                                */
/**************************************************************************************************/


/*------------------------------------------------------------------------------------------------*/
/*! \brief                 EXCLUSIVE READ/WRITE ACCESS
*
*            This define holds the value for mode: exclusive read/write access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_RW         (DEV_EXCLUSIVE | DEV_WO | DEV_RO)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 EXCLUSIVE READ/WRITE/EXECUTE ACCESS
*
*            This define holds the value for mode: exclusive read/write/execute access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_RWX        (DEV_EXCLUSIVE | DEV_WO | DEV_RO | DEV_EXE)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 SHARED WRITE ACCESS
*
*            This define holds the value for mode: shared write access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_SHWO       (DEV_SHARE | DEV_WO)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 SHARED READ ACCESS
*
*            This define holds the value for mode: shared read access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_SHRO       (DEV_SHARE | DEV_RO)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 SHARED READ/WRITE ACCESS
*
*            This define holds the value for mode: shared read/write access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_SHRW       (DEV_SHARE | DEV_WO | DEV_RO)

/*------------------------------------------------------------------------------------------------*/
/*! \brief                 SHARED READ/WRITE/EXECUTE ACCESS
*
*            This define holds the value for mode: shared read/write/execute access
*/
/*------------------------------------------------------------------------------------------------*/
#define DEV_SHRWX      (DEV_SHARE | DEV_WO | DEV_RO | DEV_EXE)


#endif  /* #ifndef _DRV_DEF_H */

