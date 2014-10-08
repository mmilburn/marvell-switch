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

#include <linux/etherdevice.h>
#include <linux/interrupt.h>

#include "os/mvOs.h"
#include "ctrlEnv/mvCtrlEnvSpec.h"
#include "eth-phy/mvEthPhyRegs.h"
#include "dsdt/gtDrvSwRegs.h"

#include <asm/../../mach-mvebu/armada-370-xp.h>

#include "gbe/mvEthRegs.h"

#include "dsdt/msApiDefs.h"
#include "dsdt/msApiPrototype.h"
#include "mv_switch.h"

/* uncomment for debug prints */
/* #define SWITCH_DEBUG */

#define SWITCH_DBG_OFF      0x0000
#define SWITCH_DBG_LOAD     0x0001
#define SWITCH_DBG_MCAST    0x0002
#define SWITCH_DBG_VLAN     0x0004
#define SWITCH_DBG_ALL      0xffff

#ifdef SWITCH_DEBUG
static MV_U32 switch_dbg = 0;
#define SWITCH_DBG(FLG, X) if ((switch_dbg & (FLG)) == (FLG)) printk X
#else
#define SWITCH_DBG(FLG, X)
#endif /* SWITCH_DEBUG */

GT_QD_DEV qddev;

static spinlock_t switch_lock;
static MV_BOOL initBridgeDone = MV_FALSE;

/*******************************************************************************
* mvEthPhyRegRead - Read from ethernet phy register.
*
* DESCRIPTION:
*       This function reads ethernet phy register.
*
* INPUT:
*       phyAddr - Phy address.
*       regOffs - Phy register offset.
*
* OUTPUT:
*       None.
*
* RETURN:
*       16bit phy register value, or 0xffff on error
*
*******************************************************************************/
MV_STATUS mvEthPhyRegRead(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 *data)
{
	MV_U32 		smiReg;
	volatile MV_U32 timeout;

	/* check parameters */
	if ((phyAddr << ETH_PHY_SMI_DEV_ADDR_OFFS) & ~ETH_PHY_SMI_DEV_ADDR_MASK) {
		mvOsPrintf("mvEthPhyRegRead: Err. Illegal PHY device address %d\n",
				phyAddr);
		return MV_FAIL;
	}
	if ((regOffs <<  ETH_PHY_SMI_REG_ADDR_OFFS) & ~ETH_PHY_SMI_REG_ADDR_MASK) {
		mvOsPrintf("mvEthPhyRegRead: Err. Illegal PHY register offset %d\n",
				regOffs);
		return MV_FAIL;
	}

	timeout = ETH_PHY_TIMEOUT;
	/* wait till the SMI is not busy*/
	do {
		/* read smi register */
		smiReg = MV_REG_READ( ETH_SMI_REG(MV_ETH_SMI_PORT));    
		if (timeout-- == 0) {
			mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
			return MV_FAIL;
		}
	} while (smiReg & ETH_PHY_SMI_BUSY_MASK);

	/* fill the phy address and regiser offset and read opcode */
	smiReg = (phyAddr <<  ETH_PHY_SMI_DEV_ADDR_OFFS) | (regOffs << ETH_PHY_SMI_REG_ADDR_OFFS)|
			   ETH_PHY_SMI_OPCODE_READ;

	/* write the smi register */
	MV_REG_WRITE( ETH_SMI_REG(MV_ETH_SMI_PORT), smiReg); 

	timeout = 0xffffffff; //ETH_PHY_TIMEOUT;

	/*wait till readed value is ready */
	do {
		/* read smi register */
		smiReg = MV_REG_READ( ETH_SMI_REG(MV_ETH_SMI_PORT));   

		if (timeout-- == 0) {
			mvOsPrintf("mvEthPhyRegRead: SMI read-valid timeout\n");
			return MV_FAIL;
		}
	} while (!(smiReg & ETH_PHY_SMI_READ_VALID_MASK));

	/* Wait for the data to update in the SMI register */
	for (timeout = 0; timeout < ETH_PHY_TIMEOUT; timeout++)
		;

        *data = (MV_U16)(MV_REG_READ( ETH_SMI_REG(MV_ETH_SMI_PORT)) & ETH_PHY_SMI_DATA_MASK); 

	return MV_OK;
}

/*******************************************************************************
* mvEthPhyRegWrite - Write to ethernet phy register.
*
* DESCRIPTION:
*       This function write to ethernet phy register.
*
* INPUT:
*       phyAddr - Phy address.
*       regOffs - Phy register offset.
*       data    - 16bit data.
*
* OUTPUT:
*       None.
*
* RETURN:
*       MV_OK if write succeed, MV_BAD_PARAM on bad parameters , MV_ERROR on error .
*		MV_TIMEOUT on timeout
*
*******************************************************************************/
MV_STATUS mvEthPhyRegWrite(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 data)
{
	MV_U32 		smiReg;
	volatile MV_U32 timeout;

	/* check parameters */
	if ((phyAddr <<  ETH_PHY_SMI_DEV_ADDR_OFFS) & ~ETH_PHY_SMI_DEV_ADDR_MASK) {
		mvOsPrintf("mvEthPhyRegWrite: Err. Illegal phy address 0x%x\n", phyAddr);
		return MV_BAD_PARAM;
	}
	if ((regOffs <<  ETH_PHY_SMI_REG_ADDR_OFFS) & ~ETH_PHY_SMI_REG_ADDR_MASK) {
		mvOsPrintf("mvEthPhyRegWrite: Err. Illegal register offset 0x%x\n", regOffs);
		return MV_BAD_PARAM;
	}

	timeout = ETH_PHY_TIMEOUT;

	/* wait till the SMI is not busy*/
	do {
		/* read smi register */
		smiReg = MV_REG_READ( ETH_SMI_REG(MV_ETH_SMI_PORT));   
		if (timeout-- == 0) {
			mvOsPrintf("mvEthPhyRegWrite: SMI busy timeout\n");
		return MV_TIMEOUT;
		}
	} while (smiReg & ETH_PHY_SMI_BUSY_MASK);

	/* fill the phy address and regiser offset and write opcode and data*/
	smiReg = (data << ETH_PHY_SMI_DATA_OFFS);
	smiReg |= (phyAddr <<  ETH_PHY_SMI_DEV_ADDR_OFFS) | (regOffs << ETH_PHY_SMI_REG_ADDR_OFFS);
	smiReg &= ~ETH_PHY_SMI_OPCODE_READ;

	/* write the smi register */
	MV_REG_WRITE( ETH_SMI_REG(MV_ETH_SMI_PORT), smiReg);  

	return MV_OK;
}

MV_STATUS mv_switch_mii_read( unsigned int phy, unsigned int reg, unsigned int *data)
{
	unsigned long	flags;
	MV_STATUS 	status;

	spin_lock_irqsave(&switch_lock, flags);
	status = mvEthPhyRegRead(phy, reg, (MV_U16 *) data);
	spin_unlock_irqrestore(&switch_lock, flags);

	return status;
}

MV_STATUS mv_switch_mii_write( unsigned int phy, unsigned int reg, unsigned int data)
{
	unsigned long	flags;
	MV_STATUS 	status;

	spin_lock_irqsave(&switch_lock, flags);
	status = mvEthPhyRegWrite(phy, reg, (MV_U16) data);
	spin_unlock_irqrestore(&switch_lock, flags);

	return status;
}

/* This macro calculates the mask for partial read /    */
/* write of register's data.                            */
#define CALC_MASK(fieldOffset,fieldLen,mask)        \
            if((fieldLen + fieldOffset) >= 16)      \
                mask = (0 - (1 << fieldOffset));    \
            else                                    \
                mask = (((1 << (fieldLen + fieldOffset))) - (1 << fieldOffset))


MV_STATUS mv_switch_mii_write_RegField( 
        IN MV_U8  	port, 
        IN MV_U8  	reg, 
        IN MV_U8  	fieldOffset, 
        IN MV_U8  	fieldLength, 
        IN MV_U16 	data)
{
	MV_U32		flags;
	MV_U16 		tmp;
	MV_U16  	mask;
	MV_STATUS 	status;

	spin_lock_irqsave(&switch_lock, flags);
	
	status = mvEthPhyRegRead( port, reg, &tmp);
	
	CALC_MASK(fieldOffset,fieldLength,mask);

        /* Set the desired bits to 0.                       */
        tmp &= ~mask;
        /* Set the given data into the above reset bits.    */
        tmp |= ((data << fieldOffset) & mask);
        
	status |= mvEthPhyRegWrite( port, reg, tmp);
	spin_unlock_irqrestore( &switch_lock, flags);

	return status;
}

int mv_switch_reg_write( int port, int reg, int type, unsigned int value)
{
	MV_STATUS status = MV_FAIL;

	switch (type) {
	case MV_SWITCH_PHY_ACCESS:
		status = mv_switch_mii_write( port, reg, value);
		break;

	case MV_SWITCH_PORT_ACCESS:
		status = mv_switch_mii_write( 0x10+port, reg, value);
		break;

	case MV_SWITCH_GLOBAL_ACCESS:
		status = mv_switch_mii_write( 0x1b, reg, value);
		break;

	case MV_SWITCH_GLOBAL2_ACCESS:
		status = mv_switch_mii_write( 0x1c, reg, value);
		break;

	case MV_SWITCH_SMI_ACCESS:
		/* port means phyAddr */
		MV_REG_WRITE( ETH_SMI_REG(MV_ETH_SMI_PORT), value);
		status = MV_OK;
		break;

	default:
		printk(KERN_ERR "%s Failed: Unexpected access type %d\n", __func__, type);
		return MV_FAIL;
	}
	
	return status;
}

int mv_switch_reg_read( int port, int reg, int type, unsigned int *value)
{
	MV_STATUS status = MV_FAIL;

	switch (type) {
	case MV_SWITCH_PHY_ACCESS:
		status = mv_switch_mii_read( port, reg, value);
		break;

	case MV_SWITCH_PORT_ACCESS:
		status = mv_switch_mii_read( 0x10+port, reg, value);
		break;

	case MV_SWITCH_GLOBAL_ACCESS:
		status = mv_switch_mii_read( 0x1b, reg, value);
		break;

	case MV_SWITCH_GLOBAL2_ACCESS:
		status = mv_switch_mii_read( 0x1c, reg, value);
		break;

	case MV_SWITCH_SMI_ACCESS:
		/* port means phyAddr */
		*value = MV_REG_READ( ETH_SMI_REG(MV_ETH_SMI_PORT)); 
		status = MV_OK;
		break;

	default:
		printk(KERN_ERR "%s Failed: Unexpected access type %d\n", __func__, type);
		return MV_FAIL;
	}

	return status;
}

int mv_switch_jumbo_mode_set( GT_QD_DEV *qd_dev, int max_size)
{
	int i;
	GT_JUMBO_MODE jumbo_mode;

	/* Set jumbo frames mode */
	if (max_size <= 1522)
		jumbo_mode = GT_JUMBO_MODE_1522;
	else if (max_size <= 2048)
		jumbo_mode = GT_JUMBO_MODE_2048;
	else
		jumbo_mode = GT_JUMBO_MODE_10240;

	for (i = 0; i < MAX_SWITCH_PORT_NUM; i++) {
		if (gsysSetJumboMode(qd_dev, i, jumbo_mode) != MV_OK) {
			printk(KERN_ERR "gsysSetJumboMode %d failed\n", jumbo_mode);
			return -1;
		}
	}
	return 0;
}


static MV_STATUS qd_dev_init(GT_QD_DEV *qd_dev)
{
	spin_lock_init(&switch_lock);

        /* Initialize dev fields.         */
        qd_dev->cpuPortNum = SWITCH_TO_CPU_LAN;
        qd_dev->maxPhyNum = 5;
        qd_dev->devGroup = 0;
        qd_dev->devStorage = 0;
        /* Assign Device Name */
        qd_dev->devName1 = 0;
       
        qd_dev->numOfPorts = 7;
        qd_dev->maxPorts = 7;
        qd_dev->maxPhyNum = 7;
        qd_dev->validPortVec = (1 << qd_dev->numOfPorts) - 1;
        qd_dev->validPhyVec = 0x7F;
        qd_dev->devName = DEV_88E6171;

    	qd_dev->use_mad = MV_FALSE;
        qd_dev->devEnabled = 1;
        qd_dev->deviceId = GT_88E6172;
        // QD_DEV init completely 

	return MV_OK;
}

int mv_switch_init(int mtu, unsigned int switch_ports_mask)
{
	MV_U16 		p;
	MV_U8 		tmp;
        GT_QD_DEV       *qd_dev = &qddev;
 
        // If the init had been done, skip all the content
        if (initBridgeDone == MV_TRUE)  return 0;

	/* general Switch initialization - relevant for all Switch devices */
        qd_dev_init( qd_dev);

	/* disable all ports */
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p))
			if (gstpSetPortState(qd_dev, p, GT_PORT_DISABLE) != MV_OK) {
				printk(KERN_ERR "gstpSetPortState failed\n");
	
				return -1;
			}
	}

	/* disable all disconnected ports (for CPU RGMII0/RGMII1) */
	for (p = 5; p <= 6; p++) {
		if (gpcsSetForceSpeed(qd_dev, p, PORT_FORCE_SPEED_1000_MBPS) != MV_OK) {
			printk(KERN_ERR "Force speed 1000mbps - Failed\n");
			return -1;
		}

		if ((gpcsSetDpxValue(qd_dev, p, MV_TRUE) != MV_OK) ||
		    (gpcsSetForcedDpx(qd_dev, p, MV_TRUE) != MV_OK)) {
			printk(KERN_ERR "Force duplex FULL - Failed\n");
			return -1;
		}

		if ((gpcsSetFCValue(qd_dev, p, MV_TRUE) != MV_OK) ||
		    (gpcsSetForcedFC(qd_dev, p, MV_TRUE) != MV_OK)) {
			printk(KERN_ERR "Force Flow Control - Failed\n");
			return -1;
		}

		if ((gpcsSetLinkValue(qd_dev, p, MV_TRUE) != MV_OK) ||
		    (gpcsSetForcedLink(qd_dev, p, MV_TRUE) != MV_OK)) {
			printk(KERN_ERR "Force Link UP - Failed\n");
			return -1;
		}
	}

	/* set all ports not to unmodify the vlan tag on egress */
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p)) {
			if (gprtSetEgressMode(qd_dev, p, GT_UNMODIFY_EGRESS) != MV_OK) {
				printk(KERN_ERR "gprtSetEgressMode GT_UNMODIFY_EGRESS failed\n");
				return -1;
			}
		}
	}

	/* initializes the PVT Table (cross-chip port based VLAN) to all one's (initial state) */
	if (gpvtInitialize(qd_dev) != MV_OK) {
		printk(KERN_ERR "gpvtInitialize failed\n");
		return -1;
	}

	/* set all ports to work in Normal mode */
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p)) {
			if (gprtSetFrameMode(qd_dev, p, GT_FRAME_MODE_NORMAL) != MV_OK) {
				printk(KERN_ERR "gprtSetFrameMode GT_FRAME_MODE_NORMAL failed\n");
				return -1;
			}
		}
	}
	
	/* specific Switch initialization according to Switch ID */
	switch (qd_dev->deviceId) {
	case GT_88E6161:
	case GT_88E6165:
	case GT_88E6171:
	case GT_88E6351:
	case GT_88E6172:
	case GT_88E6176:
		/* set Header Mode in all ports to False */
		for (p = 0; p < 5; p++) {
 			if (gprtSetHeaderMode(qd_dev, p, MV_FALSE) != MV_OK) {
				printk(KERN_ERR "gprtSetHeaderMode MV_FALSE failed\n");
				return -1;
			}
		}

		if (gprtSetHeaderMode(qd_dev, 5, MV_FALSE) != MV_OK ||
		    gprtSetHeaderMode(qd_dev, 6, MV_FALSE) != MV_OK) 
		{
			printk(KERN_ERR "gprtSetHeaderMode MV_TRUE failed\n");
			return -1;
		}

		mv_switch_jumbo_mode_set( qd_dev, mtu);
		break;

	default:
		printk(KERN_ERR "Unsupported Switch. Switch ID is 0x%X.\n", qd_dev->deviceId);
		return -1;
	}

	/* The switch CPU port is not part of the VLAN, but rather connected by tunneling to each */
	/* of the VLAN's ports. Our MAC addr will be added during start operation to the VLAN DB  */
	/* at switch level to forward packets with this DA to CPU port.                           */
	SWITCH_DBG(SWITCH_DBG_LOAD, ("Enabling Tunneling on ports: "));
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p) && (p != SWITCH_TO_CPU_WAN || p != SWITCH_TO_CPU_LAN)) {
			if (gprtSetVlanTunnel(qd_dev, p, MV_TRUE) != MV_OK) {
				printk(KERN_ERR "gprtSetVlanTunnel failed (port %d)\n", p);
				return -1;
			} else {
				SWITCH_DBG(SWITCH_DBG_LOAD, ("%d ", p));
			}
		}
	}
	
	SWITCH_DBG(SWITCH_DBG_LOAD, ("Disable 802.1Q VLAN on ports: "));
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p)) {
	            gvlnSetPortVlanDot1qMode(qd_dev, p, GT_DISABLE);
	        }
	}
	
	/* set P5(RGMII0) and P0~3 as one VLAN */
	if (gvlnSetPortVlanPortMask(qd_dev, SWITCH_TO_CPU_LAN, 0x0F) != MV_OK) {
		printk(KERN_ERR "gvlnSetPortVlanPorts failed\n");
		return -1;
	}
	
	tmp = 1;
	for (p=0; p< 4; p++) {
	    if (gvlnSetPortVlanPortMask(qd_dev, p, 0x20 | (0x0F & (~tmp))) != MV_OK) {
		printk(KERN_ERR "gvlnSetPortVlanPorts failed\n");
		return -1;
	    }
	    
	    tmp <<=1;
	}
	
	/* Set P4 and P6(RGMII1) as another port-based VLAN (for WAN) */
	if (gvlnSetPortVlanPortMask(qd_dev, SWITCH_TO_CPU_WAN, 0x10) != MV_OK) {
		printk(KERN_ERR "gvlnSetPortVlanPorts failed\n");
		return -1;
	}
	
	if (gvlnSetPortVlanPortMask(qd_dev, 4, 0x40) != MV_OK) {
		printk(KERN_ERR "gvlnSetPortVlanPorts failed\n");
		return -1;
	}	
	

	if (gfdbFlush(qd_dev, GT_FLUSH_ALL) != MV_OK)
		printk(KERN_ERR "gfdbFlush failed\n");

	/* Configure Ethernet related LEDs, currently according to Switch ID, do nothing for E61xx/63xx */

	/* enable all relevant ports (ports connected to the MAC or external ports) */
	for (p = 0; p < MAX_SWITCH_PORT_NUM; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p))
			if (gstpSetPortState(qd_dev, p, GT_PORT_FORWARDING) != MV_OK) {
				printk(KERN_ERR "gstpSetPortState failed\n");
	
				return -1;
			}
	}

	initBridgeDone = MV_TRUE;
	return 0;
}

