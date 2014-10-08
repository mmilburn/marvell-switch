/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
*******************************************************************************/

#include "dsdt/gtDrvSwRegs.h"
#include "dsdt/msApiDefs.h"
#include "os/mvCommon.h"
#include "mv_switch.h"

static MV_STATUS pvtOperationPerform
(
    IN    GT_QD_DEV           *dev,
    IN    GT_PVT_OPERATION   pvtOp,
    INOUT GT_PVT_OP_DATA     *opData
);

/*******************************************************************************
* portToSmiMapping
*
* DESCRIPTION:
*       This function mapps port to smi address
*
* INPUTS:
*        dev - device context
*       portNum - Port number to read the register for.
*        accessType - type of register (Phy, Port, or Global)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       smiAddr    - smi address.
*
*******************************************************************************/
MV_U8 portToSmiMapping
(
    IN GT_QD_DEV *dev,
    IN MV_U8    portNum,
    IN MV_U32    accessType
)
{
    MV_U8 smiAddr;

    switch(accessType)
    {
        case PHY_ACCESS:
	      smiAddr = portNum;            
              break;
        case PORT_ACCESS:
              smiAddr = portNum + PORT_REGS_START_ADDR_8PORT;
              break;
        case GLOBAL_REG_ACCESS:
              smiAddr = GLOBAL_REGS_START_ADDR_8PORT;
              break;
        case GLOBAL3_REG_ACCESS:
              smiAddr = GLOBAL_REGS_START_ADDR_8PORT + 2;
              break;
        default:
              smiAddr = GLOBAL_REGS_START_ADDR_8PORT + 1;
              break;
    }

    return smiAddr;
}

/*******************************************************************************
* gstpSetPortState
*
* DESCRIPTION:
*       This routine set the port state.
*
* INPUTS:
*       port  - the logical port number.
*       state - the port state to set.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gstpSetPortState
(
    IN GT_QD_DEV *dev,
    IN GT_LPORT           port,
    IN GT_PORT_STP_STATE  state
)
{
    MV_U16          data;           /* Data to write to register.   */
    MV_STATUS       retVal;         /* Functions return value.      */
    
    data    = state;

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* Set the port state bits.             */
    retVal = mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 0, 2, data);
    
    return retVal;
}

/*******************************************************************************
* gprtSetEgressMode
*
* DESCRIPTION:
*       This routine set the egress mode.
*
* INPUTS:
*       port - the logical port number.
*       mode - the egress mode.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gprtSetEgressMode
(
    IN GT_QD_DEV       *dev,
    IN GT_LPORT        port,
    IN GT_EGRESS_MODE  mode
)
{
    MV_STATUS       retVal;         /* Functions return value.      */
    MV_U16          data;           /* Data to be set into the      */
                                    /* register.                    */

    DBG_INFO(("gprtSetEgressMode Called.\n"));

    switch (mode)
    {
        case (GT_UNMODIFY_EGRESS):
            data = 0;
            break;

        case (GT_TAGGED_EGRESS):
            data = 2;
            break;

        case (GT_UNTAGGED_EGRESS):
            data = 1;
            break;

        case (GT_ADD_TAG):
            data = 3;
            break;
        default:
            DBG_INFO(("Failed.\n"));
            return MV_FAIL;
    }

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    retVal = mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 12, 2, data);
    if(retVal != MV_OK)
    {
        DBG_INFO(("Failed.\n"));
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return MV_OK;
}


/*******************************************************************************
* gpvtInitialize
*
* DESCRIPTION:
*       This routine initializes the PVT Table to all one's (initial state)
*
* INPUTS:
*        None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK      - on success
*       MV_FAIL    - on error
*       MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
MV_STATUS gpvtInitialize
(
    IN  GT_QD_DEV     *dev
)
{
    MV_STATUS           retVal;
    GT_PVT_OPERATION    op;

    DBG_INFO(("gpvtInitialize Called.\n"));

    /* Program Tuning register */
    op = PVT_INITIALIZE;
    retVal = pvtOperationPerform(dev,op,NULL);
    if(retVal != MV_OK)
    {
        DBG_INFO(("Failed (pvtOperationPerform returned MV_FAIL).\n"));
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return MV_OK;

}

/*******************************************************************************
* pvtOperationPerform
*
* DESCRIPTION:
*       This function accesses PVT Table
*
* INPUTS:
*       pvtOp   - The pvt operation
*       pvtData - address and data to be written into PVT
*
* OUTPUTS:
*       pvtData - data read from PVT pointed by address
*
* RETURNS:
*       MV_OK on success,
*       MV_FAIL otherwise.
*
* COMMENTS:
*
*******************************************************************************/
static MV_STATUS pvtOperationPerform
(
    IN    GT_QD_DEV           *dev,
    IN    GT_PVT_OPERATION   pvtOp,
    INOUT GT_PVT_OP_DATA     *opData
)
{
    MV_STATUS       retVal;    /* Functions return value */
    MV_U16          data;     /* temporary Data storage */

    /* Wait until the pvt in ready. */

    data = 0x8000;  // BIT15
    while(data & 0x8000)
    {
        retVal = mv_switch_mii_read( 0x1c, QD_REG_PVT_ADDR, (MV_U16 *) &data);
        if(retVal != MV_OK)
        {
            return retVal;
        }
    }

    /* Set the PVT Operation register */
    switch (pvtOp)
    {
        case PVT_INITIALIZE:
            data = (1 << 15) | (pvtOp << 12);
            retVal = mv_switch_mii_write( 0x1c, QD_REG_PVT_ADDR, data);
            if(retVal != MV_OK)
            {
                 return retVal;
            }
            break;

        case PVT_WRITE:
            data = (MV_U16)opData->pvtData;
            retVal = mv_switch_mii_write( 0x1c, QD_REG_PVT_ADDR, data);
            
            if(retVal != MV_OK)
            {
                return retVal;
            }

            data = (MV_U16)((1 << 15) | (pvtOp << 12) | opData->pvtAddr);
            retVal = mv_switch_mii_write( 0x1c, QD_REG_PVT_ADDR, data);

            if(retVal != MV_OK)
            {
                return retVal;
            }
            break;

        case PVT_READ:
            data = (MV_U16)((1 << 15) | (pvtOp << 12) | opData->pvtAddr);
            retVal = mv_switch_mii_write( 0x1c, QD_REG_PVT_ADDR, data);
            
            if(retVal != MV_OK)
            {
                return retVal;
            }
            
            data = 0x8000;  // BIT15
            
            while(data & 0x8000)
            {
                retVal = mv_switch_mii_read( 0x1c, QD_REG_PVT_ADDR, &data);
                if(retVal != MV_OK)
                {
                    return retVal;
                }
            }

            retVal = mv_switch_mii_read( 0x1c, QD_REG_PVT_DATA, &data);
            opData->pvtData = (MV_U32)data;
            if(retVal != MV_OK)
            {
                return retVal;
            }

            break;

        default:

            return MV_FAIL;
    }

    return retVal;
}

/*******************************************************************************
* gprtSetFrameMode
*
* DESCRIPTION:
*        Frmae Mode is used to define the expected Ingress and the generated Egress
*        tagging frame format for this port as follows:
*            GT_FRAME_MODE_NORMAL -
*                Normal Network mode uses industry standard IEEE 802.3ac Tagged or
*                Untagged frames. Tagged frames use an Ether Type of 0x8100.
*            GT_FRAME_MODE_DSA -
*                DSA mode uses a Marvell defined tagged frame format for
*                Chip-to-Chip and Chip-to-CPU connections.
*            GT_FRAME_MODE_PROVIDER -
*                Provider mode uses user definable Ether Types per port
*                (see gprtSetPortEType/gprtGetPortEType API).
*            GT_FRAME_MODE_ETHER_TYPE_DSA -
*                Ether Type DSA mode uses standard Marvell DSA Tagged frame info
*                flowing a user definable Ether Type. This mode allows the mixture
*                of Normal Network frames with DSA Tagged frames and is useful to
*                be used on ports that connect to a CPU.
*
* INPUTS:
*        port - the logical port number
*        mode - GT_FRAME_MODE type
*
* OUTPUTS:
*        None.
*
* RETURNS:
*        MV_OK   - on success
*        MV_FAIL - on error
*        MV_BAD_PARAM - if mode is unknown
*        MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS gprtSetFrameMode
(
    IN  GT_QD_DEV    *dev,
    IN  GT_LPORT    port,
    IN  GT_FRAME_MODE    mode
)
{
    DBG_INFO(("gprtSetFrameMode Called.\n"));

    switch (mode)
    {
        case GT_FRAME_MODE_NORMAL:
        case GT_FRAME_MODE_DSA:
        case GT_FRAME_MODE_PROVIDER:
        case GT_FRAME_MODE_ETHER_TYPE_DSA:
            break;
        default:
            DBG_INFO(("Bad Parameter\n"));
            return MV_BAD_PARAM;
    }

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* Set Frame Mode.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 8, 2, (MV_U16)mode);
}


/*******************************************************************************
* gcosSetPortDefaultTc
*
* DESCRIPTION:
*       Sets the default traffic class for a specific port.
*
* INPUTS:
*       port      - logical port number
*       trafClass - default traffic class of a port.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*       Fast Ethernet switch family supports 2 bits (0 ~ 3) while Gigabit Switch
*        family supports 3 bits (0 ~ 7)
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gcosSetPortDefaultTc
(
    IN  GT_QD_DEV *dev,
    IN GT_LPORT   port,
    IN MV_U8      trafClass
)
{
    DBG_INFO(("gcosSetPortDefaultTc Called.\n"));
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    return mv_switch_mii_write_RegField( port, QD_REG_PVID, 13, 3, trafClass);
}

/*******************************************************************************
* gqosIpPrioMapEn
*
* DESCRIPTION:
*       This routine enables the IP priority mapping.
*
* INPUTS:
*       port - the logical port number.
*       en   - MV_TRUE to Enable, MV_FALSE for otherwise.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gqosIpPrioMapEn
(
    IN  GT_QD_DEV *dev,
    IN GT_LPORT   port,
    IN MV_BOOL    en
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gqosIpPrioMapEn Called.\n"));
    /* translate bool to binary */
    BOOL_2_BIT(en, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Set the useIp.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 5, 1, data);
}


/*******************************************************************************
* gqosSetPrioMapRule
*
* DESCRIPTION:
*       This routine sets priority mapping rule.
*        If the current frame is both IEEE 802.3ac tagged and an IPv4 or IPv6,
*        and UserPrioMap (for IEEE 802.3ac) and IPPrioMap (for IP frame) are
*        enabled, then priority selection is made based on this setup.
*        If PrioMapRule is set to MV_TRUE, UserPrioMap is used.
*        If PrioMapRule is reset to MV_FALSE, IPPrioMap is used.
*
* INPUTS:
*       port - the logical port number.
*       mode - MV_TRUE for user prio rule, MV_FALSE for otherwise.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gqosSetPrioMapRule
(
    IN  GT_QD_DEV *dev,
    IN GT_LPORT   port,
    IN MV_BOOL    mode
)
{
    MV_U16          data;           /* temporary data buffer */

    DBG_INFO(("gqosSetPrioMapRule Called.\n"));
    /* translate bool to binary */
    BOOL_2_BIT(mode, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    
    /* Set the TagIfBoth.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 6, 1, data);
}

/*******************************************************************************
* gqosUserPrioMapEn
*
* DESCRIPTION:
*       This routine enables the user priority mapping.
*
* INPUTS:
*       port - the logical port number.
*       en   - MV_TRUE to Enable, MV_FALSE for otherwise.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gqosUserPrioMapEn
(
    IN  GT_QD_DEV *dev,
    IN GT_LPORT   port,
    IN MV_BOOL    en
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gqosUserPrioMapEn Called.\n"));
    /* translate bool to binary */
    BOOL_2_BIT(en, data);

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* Set the useTag.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 4, 1, data);
}

/*******************************************************************************
* gprtSetHeaderMode
*
* DESCRIPTION:
*       This routine set ingress and egress header mode of a switch port.
*
* INPUTS:
*       port - the logical port number.
*       mode - MV_TRUE for header mode  or MV_FALSE otherwise
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gprtSetHeaderMode
(
    IN  GT_QD_DEV   *dev,
    IN GT_LPORT     port,
    IN MV_BOOL      mode
)
{
    MV_U16          data;

    DBG_INFO(("gprtSetHeaderMode Called.\n"));

    /* translate BOOL to binary */
    BOOL_2_BIT(mode, data);

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* Set the header mode.            */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 11, 1, data);
}

/*******************************************************************************
* gsysSetJumboMode
*
* DESCRIPTION:
*       This routine Set the max frame size allowed to be received and transmitted
*        from or to a given port.
*
* INPUTS:
*        port - the logical port number
*       mode - GT_JUMBO_MODE (1522, 2048, or 10240)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*        MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
*******************************************************************************/
MV_STATUS gsysSetJumboMode
(
    IN  GT_QD_DEV    *dev,
    IN  GT_LPORT    port,
    IN  GT_JUMBO_MODE   mode
)
{
    DBG_INFO(("gsysSetJumboMode Called.\n"));

    if (mode > GT_JUMBO_MODE_10240)
    {
        DBG_INFO(("Bad Parameter\n"));
        return MV_BAD_PARAM;
    }

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* Set the Jumbo Fram Size bit.               */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL2, 12, 2,(MV_U16)mode);
}

/*******************************************************************************
* gprtSetVlanTunnel
*
* DESCRIPTION:
*       This routine sets the vlan tunnel mode.
*
* INPUTS:
*       port - the logical port number.
*       mode - the vlan tunnel mode.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gprtSetVlanTunnel
(
    IN GT_QD_DEV *dev,
    IN GT_LPORT  port,
    IN MV_BOOL   mode
)
{
    MV_U16          data;           /* Data to be set into the      */
                                    /* register.                    */

    DBG_INFO(("gprtSetVlanTunnel Called.\n"));

    BOOL_2_BIT(mode,data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL, 7, 1, data);
}

/*******************************************************************************
* gvlnSetPortVlanPortMask
*
* DESCRIPTION:
*       This routine sets the port VLAN group port membership list.
*
* INPUTS:
*       port        - logical port number to set.
*       portMask    - bitmap for port[0..6]
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK               - on success
*       MV_FAIL             - on error
*       MV_BAD_PARAM        - on bad parameters
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gvlnSetPortVlanPortMask
(
    IN GT_QD_DEV *dev,
    IN GT_LPORT  port,
    IN MV_U16    portMask
)
{
    DBG_INFO(("gvlnSetPortVlanPortMask Called.\n"));

    if(portMask & 0x80)
    {
        DBG_INFO(("Failed (PortMask incorrect).\n"));
        return MV_BAD_PARAM;
    }
    
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    /* numOfPorts = 3 for fullsail, = 10 for octane, = 7 for others */
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_VLAN_MAP, 0, MAX_SWITCH_PORT_NUM, portMask);
}

/********************************************************************
* gvlnSetPortVlanDot1qMode
*
* DESCRIPTION:
*       This routine sets the IEEE 802.1q mode for this port (11:10)
*
* INPUTS:
*       port    - logical port number to set.
*       mode     - 802.1q mode for this port
*
* OUTPUTS:
*       None.
*
* RETURNS:IN GT_INGRESS_MODE mode
*       MV_OK               - on success
*       MV_FAIL             - on error
*       MV_BAD_PARAM        - on bad parameters
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gvlnSetPortVlanDot1qMode
(
    IN GT_QD_DEV        *dev,
    IN GT_LPORT     	port,
    IN GT_DOT1Q_MODE    mode
)
{
    DBG_INFO(("gvlnSetPortVlanDot1qMode Called.\n"));

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);
    return mv_switch_mii_write_RegField( port, QD_REG_PORT_CONTROL2,10,2,(MV_U16)mode );
}

/*******************************************************************************
* gpcsSetForceSpeed
*
* DESCRIPTION:
*		This routine forces Speed.
*
* INPUTS:
*		port - the logical port number.
*		mode - GT_PORT_FORCED_SPEED_MODE (10, 100, 1000, or no force speed)
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetForceSpeed
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN GT_PORT_FORCED_SPEED_MODE  mode
)
{
    DBG_INFO(("gpcsSetForceSpeed Called.\n"));

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Set the Force Speed bits.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,0,2, (MV_U16)mode);
}

/*******************************************************************************
* gpcsSetDpxValue
*
* DESCRIPTION:
*		This routine sets Duplex's Forced value. This function needs to be
*		called prior to gpcsSetForcedDpx.
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force full duplex, MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetDpxValue
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetDpxValue Called.\n"));

    BOOL_2_BIT(state, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the DpxValue bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,3,1,data);
}

/*******************************************************************************
* gpcsSetForcedDpx
*
* DESCRIPTION:
*		This routine forces duplex mode. If DpxValue is set to one, calling this
*		routine with MV_TRUE will force duplex mode to be full duplex.
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force duplex mode, MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetForcedDpx
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetForcedDpx Called.\n"));

    BOOL_2_BIT(state, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the ForcedDpx bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,2,1,data);
}

/*******************************************************************************
* gpcsSetFCValue
*
* DESCRIPTION:
*		This routine sets Flow Control's force value
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force flow control enabled, MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetFCValue
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetFCValue Called.\n"));

    BOOL_2_BIT(state, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Set the FCValue bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,7,1,data);
}

/*******************************************************************************
* gpcsSetForcedFC
*
* DESCRIPTION:
*		This routine forces Flow Control. If FCValue is set to one, calling this
*		routine with MV_TRUE will force Flow Control to be enabled.
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force flow control (enable or disable), MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetForcedFC
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetForcedFC Called.\n"));

    BOOL_2_BIT(state, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the ForcedFC bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,6,1,data);
}

/*******************************************************************************
* gpcsSetLinkValue
*
* DESCRIPTION:
*		This routine sets Link's force value
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force link up, MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetLinkValue
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetLinkValue Called.\n"));

    BOOL_2_BIT(state, data);
    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the LinkValue bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,5,1,data);
}

/*******************************************************************************
* gpcsSetForcedLink
*
* DESCRIPTION:
*		This routine forces Link. If LinkValue is set to one, calling this
*		routine with MV_TRUE will force Link to be up.
*
* INPUTS:
*		port - the logical port number.
*		state - MV_TRUE to force link (up or down), MV_FALSE otherwise
*
* OUTPUTS:
*		None
*
* RETURNS:
*		MV_OK   - on success
*		MV_FAIL - on error
*		MV_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gpcsSetForcedLink
(
	IN GT_QD_DEV	*dev,
	IN GT_LPORT 	port,
	IN MV_BOOL	state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */

    DBG_INFO(("gpcsSetForcedLink Called.\n"));

    BOOL_2_BIT(state, data);

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the ForcedLink bit.  */
    return mv_switch_mii_write_RegField( port, QD_REG_PCS_CONTROL,4,1,data);
}

/*******************************************************************************
* atuOperationPerform
*
* DESCRIPTION:
*       This function is used by all ATU control functions, and is responsible
*       to write the required operation into the ATU registers.
*
* INPUTS:
*       atuOp       - The ATU operation bits to be written into the ATU
*                     operation register.
*       DBNum       - ATU Database Number for CPU accesses
*       entryPri    - The EntryPri field in the ATU Data register.
*       portVec     - The portVec field in the ATU Data register.
*       entryState  - The EntryState field in the ATU Data register.
*       atuMac      - The Mac address to be written to the ATU Mac registers.
*
* OUTPUTS:
*       entryPri    - The EntryPri field in case the atuOp is GetNext.
*       portVec     - The portVec field in case the atuOp is GetNext.
*       entryState  - The EntryState field in case the atuOp is GetNext.
*       atuMac      - The returned Mac address in case the atuOp is GetNext.
*
* RETURNS:
*       MV_OK on success,
*       MV_FAIL otherwise.
*
* COMMENTS:
*       1.  if atuMac == NULL, nothing needs to be written to ATU Mac registers.
*
*******************************************************************************/
static MV_STATUS atuOperationPerform
(
    IN      GT_QD_DEV           *dev,
    IN      GT_ATU_OPERATION    atuOp,
    INOUT   GT_EXTRA_OP_DATA    *opData,
    INOUT   GT_ATU_ENTRY        *entry
)
{
    MV_STATUS       retVal;         /* Functions return value.      */
    MV_U16          data;           /* Data to be set into the      */
                                    /* register.                    */
    MV_U16          opcodeData;           /* Data to be set into the      */
                                    /* register.                    */
    MV_U8           i;
    MV_U16          portMask;

    portMask = (1 << dev->maxPorts) - 1;

    data = 0x8000;
    while(data == 0x8000)
    {
        retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_OPERATION, &data);
        if(retVal != MV_OK)
        {
            return retVal;
        }
    }

    opcodeData = 0;

    switch (atuOp)
    {
        case LOAD_PURGE_ENTRY:
                data = (MV_U16)( (((entry->prio) & 0x3) << 14) |
                       (((entry->portVec) & portMask) << 4) |
                       (((entry->entryState.ucEntryState) & 0xF)) );
                retVal = mv_switch_mii_write( 0x1b, QD_REG_ATU_DATA_REG, data);
                if(retVal != MV_OK)
                {
                    return retVal;
                }
                /* pass thru */

        case GET_NEXT_ENTRY:
                for(i = 0; i < 3; i++)
                {
                    data=(entry->macAddr[2*i] << 8)|(entry->macAddr[1 + 2*i]);
                    retVal = mv_switch_mii_write( 0x1b, (MV_U8)(QD_REG_ATU_MAC_BASE+i), data);
                    if(retVal != MV_OK)
                    {
                        return retVal;
                    }
                }
                break;

        case FLUSH_ALL:
        case FLUSH_UNLOCKED:
        case FLUSH_ALL_IN_DB:
        case FLUSH_UNLOCKED_IN_DB:
                if (entry->entryState.ucEntryState == 0xF)
                {
                    data = (MV_U16)(0xF | ((opData->moveFrom & 0xF) << 4) | ((opData->moveTo & 0xF) << 8));
                }
                else
                {
                    data = 0;
                }
                retVal = mv_switch_mii_write( 0x1b, QD_REG_ATU_DATA_REG, data);
                   if(retVal != MV_OK)
                {
                    return retVal;
                }
                break;

        case SERVICE_VIOLATIONS:

                break;

        default :
                return MV_FAIL;
    }

    /* Set DBNum */
    retVal = mv_switch_mii_write_RegField( 0x1b, QD_REG_ATU_FID_REG,0,12,(MV_U16)(entry->DBNum & 0xFFF));
    if(retVal != MV_OK)
    {
        return retVal;
    }

    /* Set the ATU Operation register in addtion to DBNum setup  */

    opcodeData |= ((1 << 15) | (atuOp << 12));

    retVal = mv_switch_mii_write( 0x1b, QD_REG_ATU_OPERATION, opcodeData);
    if(retVal != MV_OK)
    {
        return retVal;
    }

    /* If the operation is to service violation operation wait for the response   */
    if(atuOp == SERVICE_VIOLATIONS)
    {
        /* Wait until the VTU in ready. */
        data = 0x8000;
        while(data == 0x8000)
        {
            retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_OPERATION, &data);
            if(retVal != MV_OK)
            {
                return retVal;
            }
        }

        /* get the Interrupt Cause */
        retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_OPERATION, &data);
        if(retVal != MV_OK)
        {
            return retVal;
        }
        
        data = (data >> 4) & 0x0F;

        switch (data)
        {
            case 8:    /* Age Interrupt */
                opData->intCause = GT_AGE_VIOLATION;
                break;
            case 4:    /* Member Violation */
                opData->intCause = GT_MEMBER_VIOLATION;
                break;
            case 2:    /* Miss Violation */
                opData->intCause = GT_MISS_VIOLATION;
                break;
            case 1:    /* Full Violation */
                opData->intCause = GT_FULL_VIOLATION;
                break;
            default:
                opData->intCause = 0;
                return MV_OK;
        }

        /* get the DBNum that was involved in the violation */
        entry->DBNum = 0;

        retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_FID_REG, &data);
        if(retVal != MV_OK)
        {
            return retVal;
        }
        
        data &= 0xFFF;
        entry->DBNum = (MV_U16)data;

        /* get the Source Port ID that was involved in the violation */

        retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_DATA_REG, &data);
        if(retVal != MV_OK)
        {
            return retVal;
        }

        entry->entryState.ucEntryState = data & 0xF;

        /* Get the Mac address  */
        for(i = 0; i < 3; i++)
        {
            retVal = mv_switch_mii_read( 0x1b, (MV_U8)(QD_REG_ATU_MAC_BASE+i), &data);
            if(retVal != MV_OK)
            {
                return retVal;
            }
            entry->macAddr[2*i] = data >> 8;
            entry->macAddr[1 + 2*i] = data & 0xFF;
        }

    } /* end of service violations */
    /* If the operation is a gen next operation wait for the response   */
    if(atuOp == GET_NEXT_ENTRY)
    {
        entry->trunkMember = MV_FALSE;
        entry->exPrio.useMacFPri = MV_FALSE;
        entry->exPrio.macFPri = 0;
        entry->exPrio.macQPri = 0;

        /* Wait until the ATU in ready. */
        data = 0x8000;
        while(data == 0x8000)
        {
            retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_OPERATION, &data);
            if(retVal != MV_OK)
            {
                return retVal;
            }
        }

        /* Get the Mac address  */
        for(i = 0; i < 3; i++)
        {
            retVal = mv_switch_mii_read( 0x1b, (MV_U8)(QD_REG_ATU_MAC_BASE+i), &data);
            if(retVal != MV_OK)
            {
                return retVal;
            }
            entry->macAddr[2*i] = data >> 8;
            entry->macAddr[1 + 2*i] = data & 0xFF;
        }

        retVal = mv_switch_mii_read( 0x1b, QD_REG_ATU_DATA_REG,&data);
        if(retVal != MV_OK)
        {
            return retVal;
        }
        /* Get the Atu data register fields */
        entry->prio = data >> 14;
        entry->portVec = (data >> 4) & portMask;
        entry->entryState.ucEntryState = data & 0xF;
    }

    return MV_OK;
}

/*******************************************************************************
* gfdbFlush
*
* DESCRIPTION:
*       This routine flush all or unblocked addresses from the MAC Address
*       Table.
*
* INPUTS:
*       flushCmd - the flush operation type.
*
* OUTPUTS:
*       None
*
* RETURNS:
*       MV_OK           - on success
*       MV_FAIL         - on error
*       MV_NO_RESOURCE  - failed to allocate a t2c struct
*
* COMMENTS:
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gfdbFlush
(
    IN GT_QD_DEV    *dev,
    IN GT_FLUSH_CMD flushCmd
)
{
    MV_STATUS       retVal;
    GT_ATU_ENTRY    entry;

    DBG_INFO(("gfdbFlush Called.\n"));

    entry.DBNum = 0;
    entry.entryState.ucEntryState = 0;

    if(flushCmd == GT_FLUSH_ALL)
        retVal = atuOperationPerform(dev,FLUSH_ALL,NULL,&entry);
    else
        retVal = atuOperationPerform(dev,FLUSH_UNLOCKED,NULL,&entry);

    if(retVal != MV_OK)
    {
        DBG_INFO(("Failed.\n"));
    }
    
    return retVal;
}

MV_STATUS gprtPortPowerGet( IN GT_QD_DEV  *dev, IN GT_LPORT port)
{
    MV_U16          data;

    mv_switch_mii_read( port, QD_PHY_CONTROL_REG, &data);
     
    data = (data >> 11) & 1;   
    return !data;
}

/*******************************************************************************
* gprtPortPowerSet
*
* DESCRIPTION:
*       This routine resets port disables/enables port power
*
* INPUTS:
*       port  - the logical port number.
*       onoff - 0 port power down, otherwise port power up
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gprtPortPowerSet(IN GT_QD_DEV  *dev,
			   IN GT_LPORT   port,
			   IN MV_BOOL    onoff)
{
      
        // Set Phy to page#2
        mv_switch_mii_write_RegField( port, 22, 0, 8, 2);
        
	if (onoff != MV_TRUE) {
		mv_switch_mii_write_RegField( port, QD_PHY_SPEC_CONTROL_REG, 3, 1, 0);
        	
        	// Set Phy to page#0
        	mv_switch_mii_write_RegField( port, 22, 0, 8, 0);
		
		/* Page 0, register 16, bits 3,2. Copper Transmitter disable, Power down */
		mv_switch_mii_write_RegField( port, QD_PHY_SPEC_CONTROL_REG, 2, 2, 3);
		
		/* Register 0, bit 11, Power Down */
		mv_switch_mii_write_RegField( port, QD_PHY_CONTROL_REG, 11, 1, 1);
	} else {
		/* Page 2, register 16, bit 3 GMII intefrace power down */
		mv_switch_mii_write_RegField( port, QD_PHY_SPEC_CONTROL_REG, 3, 1, 1);
		
        	// Set Phy to page#0
        	mv_switch_mii_write_RegField( port, 22, 0, 8, 0);

		/* Page 0, register 16, bits 3,2. Copper Transmitter disable, Power down */
		mv_switch_mii_write_RegField( port, QD_PHY_SPEC_CONTROL_REG, 2, 2, 0);
		
		/* Register 0, bit 11, Power Down */
		mv_switch_mii_write_RegField( port, QD_PHY_CONTROL_REG, 11, 1, 0);
	}
	return 0;
}

/*******************************************************************************
* gprtGetLinkState
*
* DESCRIPTION:
*       This routine retrives the link state.
*
* INPUTS:
*       port - the logical port number.
*
* OUTPUTS:
*       state - MV_TRUE for Up  or MV_FALSE otherwise
*
* RETURNS:
*       MV_OK   - on success
*       MV_FAIL - on error
*
* COMMENTS:
*
*
* GalTis:
*
*******************************************************************************/
MV_STATUS gprtGetLinkState
(
    IN  GT_QD_DEV *dev,
    IN  GT_LPORT  port,
    OUT MV_BOOL   *state
)
{
    MV_U16          data;           /* Used to poll the SWReset bit */
    MV_STATUS       retVal;         /* Functions return value.      */

    DBG_INFO(("gprtGetLinkState Called.\n"));

    port = CALC_SMI_DEV_ADDR(dev, port, PORT_ACCESS);

    /* Get the force flow control bit.  */
    retVal = mv_switch_mii_read( port, QD_REG_PORT_STATUS, &data);

    data = (data >> 11) & 1;
    /* translate binary to BOOL  */
    BIT_2_BOOL(data, *state);

    /* return */
    return retVal;
}