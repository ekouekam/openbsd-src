/*	$OpenBSD: ixgbe_82599.c,v 1.9 2012/12/17 12:28:06 mikeb Exp $	*/

/******************************************************************************

  Copyright (c) 2001-2009, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: src/sys/dev/ixgbe/ixgbe_82599.c,v 1.3 2009/12/07 21:30:54 jfv Exp $*/

#include <dev/pci/ixgbe.h>
#include <dev/pci/ixgbe_type.h>

int32_t ixgbe_init_ops_82599(struct ixgbe_hw *hw);
int32_t ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw,
				      ixgbe_link_speed *speed,
				      int *autoneg);
enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw);
void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
int32_t ixgbe_setup_mac_link_multispeed_fiber(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, int autoneg,
				     int autoneg_wait_to_complete);
int32_t ixgbe_setup_mac_link_smartspeed(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, int autoneg,
				     int autoneg_wait_to_complete);
int32_t ixgbe_start_mac_link_82599(struct ixgbe_hw *hw,
				int autoneg_wait_to_complete);
int32_t ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed,
				     int autoneg,
				     int autoneg_wait_to_complete);
int32_t ixgbe_setup_copper_link_82599(struct ixgbe_hw *hw,
					       ixgbe_link_speed speed,
					       int autoneg,
					       int autoneg_wait_to_complete);
int32_t ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw);
void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw);
int32_t ixgbe_reset_hw_82599(struct ixgbe_hw *hw);
int32_t ixgbe_read_analog_reg8_82599(struct ixgbe_hw *hw, uint32_t reg, uint8_t *val);
int32_t ixgbe_write_analog_reg8_82599(struct ixgbe_hw *hw, uint32_t reg, uint8_t val);
int32_t ixgbe_start_hw_82599(struct ixgbe_hw *hw);
int32_t ixgbe_identify_phy_82599(struct ixgbe_hw *hw);
int32_t ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw);
uint32_t ixgbe_get_supported_physical_layer_82599(struct ixgbe_hw *hw);
int32_t ixgbe_enable_rx_dma_82599(struct ixgbe_hw *hw, uint32_t regval);
int32_t ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw);
int ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw);

void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;

	DEBUGFUNC("ixgbe_init_mac_link_ops_82599");

	/* enable the laser control functions for SFP+ fiber */
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_fiber) {
		mac->ops.disable_tx_laser =
				       &ixgbe_disable_tx_laser_multispeed_fiber;
		mac->ops.enable_tx_laser =
					&ixgbe_enable_tx_laser_multispeed_fiber;
		mac->ops.flap_tx_laser = &ixgbe_flap_tx_laser_multispeed_fiber;

	} else {
		mac->ops.disable_tx_laser = NULL;
		mac->ops.enable_tx_laser = NULL;
		mac->ops.flap_tx_laser = NULL;
	}

	if (hw->phy.multispeed_fiber) {
		/* Set up dual speed SFP+ support */
		mac->ops.setup_link = &ixgbe_setup_mac_link_multispeed_fiber;
	} else {
		if ((ixgbe_hw0(hw, get_media_type) == ixgbe_media_type_backplane) &&
		     (hw->phy.smart_speed == ixgbe_smart_speed_auto ||
		      hw->phy.smart_speed == ixgbe_smart_speed_on) &&
		      !ixgbe_verify_lesm_fw_enabled_82599(hw)) {
			mac->ops.setup_link = &ixgbe_setup_mac_link_smartspeed;
		} else {
			mac->ops.setup_link = &ixgbe_setup_mac_link_82599;
		}
	}
}

/**
 *  ixgbe_init_phy_ops_82599 - PHY/SFP specific init
 *  @hw: pointer to hardware structure
 *
 *  Initialize any function pointers that were not able to be
 *  set during init_shared_code because the PHY/SFP type was
 *  not known.  Perform the SFP init if necessary.
 *
 **/
int32_t ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	int32_t ret_val = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_init_phy_ops_82599");

	/* Identify the PHY or SFP module */
	ret_val = phy->ops.identify(hw);
	if (ret_val == IXGBE_ERR_SFP_NOT_SUPPORTED)
		goto init_phy_ops_out;

	/* Setup function pointers based on detected SFP module and speeds */
	ixgbe_init_mac_link_ops_82599(hw);
	if (hw->phy.sfp_type != ixgbe_sfp_type_unknown)
		hw->phy.ops.reset = NULL;

	/* If copper media, overwrite with copper function pointers */
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper) {
		mac->ops.setup_link = &ixgbe_setup_copper_link_82599;
		mac->ops.get_link_capabilities =
				  &ixgbe_get_copper_link_capabilities_generic;
	}

	/* Set necessary function pointers based on phy type */
	switch (hw->phy.type) {
	case ixgbe_phy_tn:
		phy->ops.setup_link = &ixgbe_setup_phy_link_tnx;
		phy->ops.check_link = &ixgbe_check_phy_link_tnx;
		phy->ops.get_firmware_version =
			     &ixgbe_get_phy_firmware_version_tnx;
		break;
	default:
		break;
	}
init_phy_ops_out:
	return ret_val;
}

int32_t ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw)
{
	int32_t ret_val = IXGBE_SUCCESS;
	uint32_t reg_anlp1 = 0;
	uint32_t i = 0;
	uint16_t list_offset, data_offset, data_value;

	DEBUGFUNC("ixgbe_setup_sfp_modules_82599");

	if (hw->phy.sfp_type != ixgbe_sfp_type_unknown) {
		ixgbe_init_mac_link_ops_82599(hw);

		hw->phy.ops.reset = NULL;

		ret_val = ixgbe_get_sfp_init_sequence_offsets(hw, &list_offset,
							      &data_offset);
		if (ret_val != IXGBE_SUCCESS)
			goto setup_sfp_out;

		/* PHY config will finish before releasing the semaphore */
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
							IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != IXGBE_SUCCESS) {
			ret_val = IXGBE_ERR_SWFW_SYNC;
			goto setup_sfp_out;
		}

		hw->eeprom.ops.read(hw, ++data_offset, &data_value);
		while (data_value != 0xffff) {
			IXGBE_WRITE_REG(hw, IXGBE_CORECTL, data_value);
			IXGBE_WRITE_FLUSH(hw);
			hw->eeprom.ops.read(hw, ++data_offset, &data_value);
		}

		/* Release the semaphore */
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
		/* Delay obtaining semaphore again to allow FW access */
		msec_delay(hw->eeprom.semaphore_delay);

		/* Now restart DSP by setting Restart_AN and clearing LMS */
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, ((IXGBE_READ_REG(hw,
				IXGBE_AUTOC) & ~IXGBE_AUTOC_LMS_MASK) |
				IXGBE_AUTOC_AN_RESTART));

		/* Wait for AN to leave state 0 */
		for (i = 0; i < 10; i++) {
			msec_delay(4);
			reg_anlp1 = IXGBE_READ_REG(hw, IXGBE_ANLP1);
			if (reg_anlp1 & IXGBE_ANLP1_AN_STATE_MASK)
				break;
		}
		if (!(reg_anlp1 & IXGBE_ANLP1_AN_STATE_MASK)) {
			DEBUGOUT("sfp module setup not complete\n");
			ret_val = IXGBE_ERR_SFP_SETUP_NOT_COMPLETE;
			goto setup_sfp_out;
		}

		/* Restart DSP by setting Restart_AN and return to SFI mode */
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, (IXGBE_READ_REG(hw,
				IXGBE_AUTOC) | IXGBE_AUTOC_LMS_10G_SERIAL |
				IXGBE_AUTOC_AN_RESTART));
	}

setup_sfp_out:
	return ret_val;
}

/**
 *  ixgbe_init_ops_82599 - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type for 82599.
 *  Does not touch the hardware.
 **/

int32_t ixgbe_init_ops_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	int32_t ret_val;

	DEBUGFUNC("ixgbe_init_ops_82599");

	ret_val = ixgbe_init_phy_ops_generic(hw);
	ret_val = ixgbe_init_ops_generic(hw);

	/* PHY */
	phy->ops.identify = &ixgbe_identify_phy_82599;
	phy->ops.init = &ixgbe_init_phy_ops_82599;

	/* MAC */
	mac->ops.reset_hw = &ixgbe_reset_hw_82599;
	mac->ops.enable_relaxed_ordering = &ixgbe_enable_relaxed_ordering_gen2;
	mac->ops.get_media_type = &ixgbe_get_media_type_82599;
	mac->ops.get_supported_physical_layer =
				    &ixgbe_get_supported_physical_layer_82599;
	mac->ops.enable_rx_dma = &ixgbe_enable_rx_dma_82599;
	mac->ops.read_analog_reg8 = &ixgbe_read_analog_reg8_82599;
	mac->ops.write_analog_reg8 = &ixgbe_write_analog_reg8_82599;
	mac->ops.start_hw = &ixgbe_start_hw_82599;

	mac->ops.get_device_caps = &ixgbe_get_device_caps_generic;
#if 0
	mac->ops.get_san_mac_addr = &ixgbe_get_san_mac_addr_generic;
	mac->ops.set_san_mac_addr = &ixgbe_set_san_mac_addr_generic;
	mac->ops.get_wwn_prefix = &ixgbe_get_wwn_prefix_generic;
	mac->ops.get_fcoe_boot_status = &ixgbe_get_fcoe_boot_status_generic;
#endif

	/* RAR, Multicast, VLAN */
	mac->ops.set_vmdq = &ixgbe_set_vmdq_generic;
	mac->ops.clear_vmdq = &ixgbe_clear_vmdq_generic;
	mac->ops.insert_mac_addr = &ixgbe_insert_mac_addr_generic;
	mac->rar_highwater = 1;
	mac->ops.set_vfta = &ixgbe_set_vfta_generic;
	mac->ops.clear_vfta = &ixgbe_clear_vfta_generic;
	mac->ops.init_uta_tables = &ixgbe_init_uta_tables_generic;
	mac->ops.setup_sfp = &ixgbe_setup_sfp_modules_82599;
#if 0
	mac->ops.set_mac_anti_spoofing = &ixgbe_set_mac_anti_spoofing;
	mac->ops.set_vlan_anti_spoofing = &ixgbe_set_vlan_anti_spoofing;
#endif

	/* Link */
	mac->ops.get_link_capabilities = &ixgbe_get_link_capabilities_82599;
	mac->ops.check_link = &ixgbe_check_mac_link_generic;
	ixgbe_init_mac_link_ops_82599(hw);

	mac->mcft_size        = 128;
	mac->vft_size         = 128;
	mac->num_rar_entries  = 128;
	mac->rx_pb_size       = 512;
	mac->max_tx_queues    = 128;
	mac->max_rx_queues    = 128;
	mac->max_msix_vectors = ixgbe_get_pcie_msix_count_generic(hw);

	hw->mbx.ops.init_params = ixgbe_init_mbx_params_pf;

	return ret_val;
}

/**
 *  ixgbe_get_link_capabilities_82599 - Determines link capabilities
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @negotiation: TRUE when autoneg or autotry is enabled
 *
 *  Determines the link capabilities by reading the AUTOC register.
 **/
int32_t ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw,
				      ixgbe_link_speed *speed,
				      int *negotiation)
{
	int32_t status = IXGBE_SUCCESS;
	uint32_t autoc = 0;

	DEBUGFUNC("ixgbe_get_link_capabilities_82599");

	/* Check if 1G SFP module. */
	if (hw->phy.sfp_type == ixgbe_sfp_type_1g_cu_core0 ||
	    hw->phy.sfp_type == ixgbe_sfp_type_1g_cu_core1) {
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = TRUE;
		goto out;
	}

	/*
	 * Determine link capabilities based on the stored value of AUTOC,
	 * which represents EEPROM defaults.  If AUTOC value has not
	 * been stored, use the current register values.
	 */
	if (hw->mac.orig_link_settings_stored)
		autoc = hw->mac.orig_autoc;
	else
		autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = FALSE;
		break;

	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*negotiation = FALSE;
		break;

	case IXGBE_AUTOC_LMS_1G_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = TRUE;
		break;

	case IXGBE_AUTOC_LMS_10G_SERIAL:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*negotiation = FALSE;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR:
	case IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = TRUE;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = TRUE;
		break;

	case IXGBE_AUTOC_LMS_SGMII_1G_100M:
		*speed = IXGBE_LINK_SPEED_1GB_FULL | IXGBE_LINK_SPEED_100_FULL;
		*negotiation = FALSE;
		break;

	default:
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
		break;
	}

	if (hw->phy.multispeed_fiber) {
		*speed |= IXGBE_LINK_SPEED_10GB_FULL |
			  IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = TRUE;
	}

out:
	return status;
}

/**
 *  ixgbe_get_media_type_82599 - Get media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 **/
enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	DEBUGFUNC("ixgbe_get_media_type_82599");

	/* Detect if there is a copper PHY attached. */
	switch (hw->phy.type) {
	case ixgbe_phy_cu_unknown:
	case ixgbe_phy_tn:
		media_type = ixgbe_media_type_copper;
		goto out;
	default:
		break;
	}

	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_KX4:
	case IXGBE_DEV_ID_82599_KX4_MEZZ:
	case IXGBE_DEV_ID_82599_COMBO_BACKPLANE:
	case IXGBE_DEV_ID_82599_KR:
	case IXGBE_DEV_ID_82599_BACKPLANE_FCOE:
	case IXGBE_DEV_ID_82599_XAUI_LOM:
		/* Default device ID is mezzanine card KX/KX4 */
		media_type = ixgbe_media_type_backplane;
		break;
	case IXGBE_DEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_SFP_FCOE:
	case IXGBE_DEV_ID_82599_SFP_EM:
	case IXGBE_DEV_ID_82599_SFP_SF2:
	case IXGBE_DEV_ID_82599EN_SFP:
		media_type = ixgbe_media_type_fiber;
		break;
	case IXGBE_DEV_ID_82599_CX4:
		media_type = ixgbe_media_type_cx4;
		break;
	case IXGBE_DEV_ID_82599_T3_LOM:
		media_type = ixgbe_media_type_copper;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}
out:
	return media_type;
}

/**
 *  ixgbe_start_mac_link_82599 - Setup MAC link settings
 *  @hw: pointer to hardware structure
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Configures link settings based on values in the ixgbe_hw struct.
 *  Restarts the link.  Performs autonegotiation if needed.
 **/
int32_t ixgbe_start_mac_link_82599(struct ixgbe_hw *hw,
			       int autoneg_wait_to_complete)
{
	uint32_t autoc_reg;
	uint32_t links_reg;
	uint32_t i;
	int32_t status = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_start_mac_link_82599");


	/* Restart link */
	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);

	/* Only poll for autoneg to complete if specified to do so */
	if (autoneg_wait_to_complete) {
		if ((autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR ||
		    (autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
		    (autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
			links_reg = 0; /* Just in case Autoneg time = 0 */
			for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
				links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
				if (links_reg & IXGBE_LINKS_KX_AN_COMP)
					break;
				msec_delay(100);
			}
			if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
				status = IXGBE_ERR_AUTONEG_NOT_COMPLETE;
				DEBUGOUT("Autoneg did not complete.\n");
			}
		}
	}

	/* Add delay to filter out noises during initial link setup */
	msec_delay(50);

	return status;
}

/**
 *  ixgbe_disable_tx_laser_multispeed_fiber - Disable Tx laser
 *  @hw: pointer to hardware structure
 *
 *  The base drivers may require better control over SFP+ module
 *  PHY states.  This includes selectively shutting down the Tx
 *  laser on the PHY, effectively halting physical link.
 **/
void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	uint32_t esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Disable tx laser; allow 100us to go dark per spec */
	esdp_reg |= IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	usec_delay(100);
}

/**
 *  ixgbe_enable_tx_laser_multispeed_fiber - Enable Tx laser
 *  @hw: pointer to hardware structure
 *
 *  The base drivers may require better control over SFP+ module
 *  PHY states.  This includes selectively turning on the Tx
 *  laser on the PHY, effectively starting physical link.
 **/
void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	uint32_t esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Enable tx laser; allow 100ms to light up */
	esdp_reg &= ~IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	msec_delay(100);
}

/**
 *  ixgbe_flap_tx_laser_multispeed_fiber - Flap Tx laser
 *  @hw: pointer to hardware structure
 *
 *  When the driver changes the link speeds that it can support,
 *  it sets autotry_restart to TRUE to indicate that we need to
 *  initiate a new autotry session with the link partner.  To do
 *  so, we set the speed then disable and re-enable the tx laser, to
 *  alert the link partner that it also needs to restart autotry on its
 *  end.  This is consistent with TRUE clause 37 autoneg, which also
 *  involves a loss of signal.
 **/
void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	DEBUGFUNC("ixgbe_flap_tx_laser_multispeed_fiber");

	if (hw->mac.autotry_restart) {
		ixgbe_disable_tx_laser_multispeed_fiber(hw);
		ixgbe_enable_tx_laser_multispeed_fiber(hw);
		hw->mac.autotry_restart = FALSE;
	}
}

/**
 *  ixgbe_setup_mac_link_multispeed_fiber - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
int32_t ixgbe_setup_mac_link_multispeed_fiber(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, int autoneg,
				     int autoneg_wait_to_complete)
{
	int32_t status = IXGBE_SUCCESS;
	ixgbe_link_speed link_speed;
	ixgbe_link_speed highest_link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	uint32_t speedcnt = 0;
	uint32_t esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);
	uint32_t i = 0;
	int link_up = FALSE;
	int negotiation;

	DEBUGFUNC("ixgbe_setup_mac_link_multispeed_fiber");

	/* Mask off requested but non-supported speeds */
	status = ixgbe_hw(hw, get_link_capabilities, &link_speed, &negotiation);
	if (status != IXGBE_SUCCESS)
		return status;

	speed &= link_speed;

	/*
	 * Try each speed one by one, highest priority first.  We do this in
	 * software because 10gb fiber doesn't support speed autonegotiation.
	 */
	if (speed & IXGBE_LINK_SPEED_10GB_FULL) {
		speedcnt++;
		highest_link_speed = IXGBE_LINK_SPEED_10GB_FULL;

		/* If we already have link at this speed, just jump out */
		status = ixgbe_hw(hw, check_link, &link_speed, &link_up, FALSE);
		if (status != IXGBE_SUCCESS)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_10GB_FULL) && link_up)
			goto out;

		/* Set the module link speed */
		esdp_reg |= (IXGBE_ESDP_SDP5_DIR | IXGBE_ESDP_SDP5);
		IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
		IXGBE_WRITE_FLUSH(hw);

		/* Allow module to change analog characteristics (1G->10G) */
		msec_delay(40);

		status = ixgbe_setup_mac_link_82599(hw,
						IXGBE_LINK_SPEED_10GB_FULL,
						autoneg,
						autoneg_wait_to_complete);
		if (status != IXGBE_SUCCESS)
			return status;

		/* Flap the tx laser if it has not already been done */
		ixgbe_hw(hw, flap_tx_laser);

		/*
		 * Wait for the controller to acquire link.  Per IEEE 802.3ap,
		 * Section 73.10.2, we may have to wait up to 500ms if KR is
		 * attempted.  82599 uses the same timing for 10g SFI.
		 */
		for (i = 0; i < 5; i++) {
			/* Wait for the link partner to also set speed */
			msec_delay(100);

			/* If we have link, just jump out */
			status = ixgbe_hw(hw, check_link, &link_speed,
						  &link_up, FALSE);
			if (status != IXGBE_SUCCESS)
				return status;

			if (link_up)
				goto out;
		}
	}

	if (speed & IXGBE_LINK_SPEED_1GB_FULL) {
		speedcnt++;
		if (highest_link_speed == IXGBE_LINK_SPEED_UNKNOWN)
			highest_link_speed = IXGBE_LINK_SPEED_1GB_FULL;

		/* If we already have link at this speed, just jump out */
		status = ixgbe_hw(hw, check_link, &link_speed, &link_up, FALSE);
		if (status != IXGBE_SUCCESS)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_1GB_FULL) && link_up)
			goto out;

		/* Set the module link speed */
		esdp_reg &= ~IXGBE_ESDP_SDP5;
		esdp_reg |= IXGBE_ESDP_SDP5_DIR;
		IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
		IXGBE_WRITE_FLUSH(hw);

		/* Allow module to change analog characteristics (10G->1G) */
		msec_delay(40);

		status = ixgbe_setup_mac_link_82599(hw,
						    IXGBE_LINK_SPEED_1GB_FULL,
						    autoneg,
						    autoneg_wait_to_complete);
		if (status != IXGBE_SUCCESS)
			return status;

		/* Flap the tx laser if it has not already been done */
		ixgbe_hw(hw, flap_tx_laser);

		/* Wait for the link partner to also set speed */
		msec_delay(100);

		/* If we have link, just jump out */
		status = ixgbe_hw(hw, check_link, &link_speed, &link_up, FALSE);
		if (status != IXGBE_SUCCESS)
			return status;

		if (link_up)
			goto out;
	}

	/*
	 * We didn't get link.  Configure back to the highest speed we tried,
	 * (if there was more than one).  We call ourselves back with just the
	 * single highest speed that the user requested.
	 */
	if (speedcnt > 1)
		status = ixgbe_setup_mac_link_multispeed_fiber(hw,
			highest_link_speed, autoneg, autoneg_wait_to_complete);

out:
	/* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_10GB_FULL;

	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_1GB_FULL;

	return status;
}

/**
 *  ixgbe_setup_mac_link_smartspeed - Set MAC link speed using SmartSpeed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Implements the Intel SmartSpeed algorithm.
 **/
int32_t ixgbe_setup_mac_link_smartspeed(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, int autoneg,
				     int autoneg_wait_to_complete)
{
	int32_t status = IXGBE_SUCCESS;
	ixgbe_link_speed link_speed;
	int32_t i, j;
	int link_up = FALSE;
	uint32_t autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	DEBUGFUNC("ixgbe_setup_mac_link_smartspeed");

	 /* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_10GB_FULL;

	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_1GB_FULL;

	if (speed & IXGBE_LINK_SPEED_100_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_100_FULL;

	/*
	 * Implement Intel SmartSpeed algorithm.  SmartSpeed will reduce the
	 * autoneg advertisement if link is unable to be established at the
	 * highest negotiated rate.  This can sometimes happen due to integrity
	 * issues with the physical media connection.
	 */

	/* First, try to get link with full advertisement */
	hw->phy.smart_speed_active = FALSE;
	for (j = 0; j < IXGBE_SMARTSPEED_MAX_RETRIES; j++) {
		status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
						    autoneg_wait_to_complete);
		if (status != IXGBE_SUCCESS)
			goto out;

		/*
		 * Wait for the controller to acquire link.  Per IEEE 802.3ap,
		 * Section 73.10.2, we may have to wait up to 500ms if KR is
		 * attempted, or 200ms if KX/KX4/BX/BX4 is attempted, per
		 * Table 9 in the AN MAS.
		 */
		for (i = 0; i < 5; i++) {
			msec_delay(100);

			/* If we have link, just jump out */
			status = ixgbe_hw(hw, check_link, &link_speed, &link_up,
						  FALSE);
			if (status != IXGBE_SUCCESS)
				goto out;

			if (link_up)
				goto out;
		}
	}

	/*
	 * We didn't get link.  If we advertised KR plus one of KX4/KX
	 * (or BX4/BX), then disable KR and try again.
	 */
	if (((autoc_reg & IXGBE_AUTOC_KR_SUPP) == 0) ||
	    ((autoc_reg & IXGBE_AUTOC_KX4_KX_SUPP_MASK) == 0))
		goto out;

	/* Turn SmartSpeed on to disable KR support */
	hw->phy.smart_speed_active = TRUE;
	status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
					    autoneg_wait_to_complete);
	if (status != IXGBE_SUCCESS)
		goto out;

	/*
	 * Wait for the controller to acquire link.  600ms will allow for
	 * the AN link_fail_inhibit_timer as well for multiple cycles of
	 * parallel detect, both 10g and 1g. This allows for the maximum
	 * connect attempts as defined in the AN MAS table 73-7.
	 */
	for (i = 0; i < 6; i++) {
		msec_delay(100);

		/* If we have link, just jump out */
		status = ixgbe_hw(hw, check_link, &link_speed, &link_up, FALSE);
		if (status != IXGBE_SUCCESS)
			goto out;

		if (link_up)
			goto out;
	}

	/* We didn't get link.  Turn SmartSpeed back off. */
	hw->phy.smart_speed_active = FALSE;
	status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
					    autoneg_wait_to_complete);

out:
	if (link_up && (link_speed == IXGBE_LINK_SPEED_1GB_FULL))
		DEBUGOUT("Smartspeed has downgraded the link speed "
		"from the maximum advertised\n");
	return status;
}

/**
 *  ixgbe_setup_mac_link_82599 - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE when waiting for completion is needed
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
int32_t ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, int autoneg,
				     int autoneg_wait_to_complete)
{
	int32_t status = IXGBE_SUCCESS;
	uint32_t autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	uint32_t autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	uint32_t start_autoc = autoc;
	uint32_t orig_autoc = 0;
	uint32_t link_mode = autoc & IXGBE_AUTOC_LMS_MASK;
	uint32_t pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;
	uint32_t pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	uint32_t links_reg;
	uint32_t i;
	ixgbe_link_speed link_capabilities = IXGBE_LINK_SPEED_UNKNOWN;

	DEBUGFUNC("ixgbe_setup_mac_link_82599");

	/* Check to see if speed passed in is supported. */
	status = ixgbe_hw(hw, get_link_capabilities, &link_capabilities, &autoneg);
	if (status != IXGBE_SUCCESS)
		goto out;

	speed &= link_capabilities;

	if (speed == IXGBE_LINK_SPEED_UNKNOWN) {
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
	}

	/* Use stored value (EEPROM defaults) of AUTOC to find KR/KX4 support*/
	if (hw->mac.orig_link_settings_stored)
		orig_autoc = hw->mac.orig_autoc;
	else
		orig_autoc = autoc;

	if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
		/* Set KX4/KX/KR support according to speed requested */
		autoc &= ~(IXGBE_AUTOC_KX4_KX_SUPP_MASK | IXGBE_AUTOC_KR_SUPP);
		if (speed & IXGBE_LINK_SPEED_10GB_FULL) {
			if (orig_autoc & IXGBE_AUTOC_KX4_SUPP)
				autoc |= IXGBE_AUTOC_KX4_SUPP;
			if ((orig_autoc & IXGBE_AUTOC_KR_SUPP) &&
			    (hw->phy.smart_speed_active == FALSE))
				autoc |= IXGBE_AUTOC_KR_SUPP;
		}
		if (speed & IXGBE_LINK_SPEED_1GB_FULL)
			autoc |= IXGBE_AUTOC_KX_SUPP;
	} else if ((pma_pmd_1g == IXGBE_AUTOC_1G_SFI) &&
		 (link_mode == IXGBE_AUTOC_LMS_1G_LINK_NO_AN ||
		  link_mode == IXGBE_AUTOC_LMS_1G_AN)) {
		/* Switch from 1G SFI to 10G SFI if requested */
		if ((speed == IXGBE_LINK_SPEED_10GB_FULL) &&
		    (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			autoc |= IXGBE_AUTOC_LMS_10G_SERIAL;
		}
	} else if ((pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI) &&
		   (link_mode == IXGBE_AUTOC_LMS_10G_SERIAL)) {
		/* Switch from 10G SFI to 1G SFI if requested */
		if ((speed == IXGBE_LINK_SPEED_1GB_FULL) &&
		    (pma_pmd_1g == IXGBE_AUTOC_1G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			if (autoneg)
				autoc |= IXGBE_AUTOC_LMS_1G_AN;
			else
				autoc |= IXGBE_AUTOC_LMS_1G_LINK_NO_AN;
		}
	}

	if (autoc != start_autoc) {
		/* Restart link */
		autoc |= IXGBE_AUTOC_AN_RESTART;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);

		/* Only poll for autoneg to complete if specified to do so */
		if (autoneg_wait_to_complete) {
			if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
				links_reg = 0; /*Just in case Autoneg time=0*/
				for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
					links_reg =
					       IXGBE_READ_REG(hw, IXGBE_LINKS);
					if (links_reg & IXGBE_LINKS_KX_AN_COMP)
						break;
					msec_delay(100);
				}
				if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
					status =
						IXGBE_ERR_AUTONEG_NOT_COMPLETE;
					DEBUGOUT("Autoneg did not complete.\n");
				}
			}
		}

		/* Add delay to filter out noises during initial link setup */
		msec_delay(50);
	}

out:
	return status;
}

/**
 *  ixgbe_setup_copper_link_82599 - Set the PHY autoneg advertised field
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg: TRUE if autonegotiation enabled
 *  @autoneg_wait_to_complete: TRUE if waiting is needed to complete
 *
 *  Restarts link on PHY and MAC based on settings passed in.
 **/
int32_t ixgbe_setup_copper_link_82599(struct ixgbe_hw *hw,
					       ixgbe_link_speed speed,
					       int autoneg,
					       int autoneg_wait_to_complete)
{
	int32_t status;

	DEBUGFUNC("ixgbe_setup_copper_link_82599");

	/* Setup the PHY according to input speed */
	status = hw->phy.ops.setup_link_speed(hw, speed, autoneg,
					      autoneg_wait_to_complete);
	/* Set up MAC */
	ixgbe_start_mac_link_82599(hw, autoneg_wait_to_complete);

	return status;
}
/**
 *  ixgbe_reset_hw_82599 - Perform hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks
 *  and clears all interrupts, perform a PHY reset, and perform a link (MAC)
 *  reset.
 **/
int32_t ixgbe_reset_hw_82599(struct ixgbe_hw *hw)
{
	int32_t status = IXGBE_SUCCESS;
	uint32_t ctrl;
	uint32_t i;
	uint32_t autoc;
	uint32_t autoc2;

	DEBUGFUNC("ixgbe_reset_hw_82599");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.ops.stop_adapter(hw);

	/* PHY ops must be identified and initialized prior to reset */

	/* Identify PHY and related function pointers */
	status = hw->phy.ops.init(hw);

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		goto reset_hw_out;

	/* Setup SFP module if there is one present. */
	if (hw->phy.sfp_setup_needed) {
		status = hw->mac.ops.setup_sfp(hw);
		hw->phy.sfp_setup_needed = FALSE;
	}

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		goto reset_hw_out;

	/* Reset PHY */
	if (hw->phy.reset_disable == FALSE && hw->phy.ops.reset != NULL)
		hw->phy.ops.reset(hw);

	/*
	 * Prevent the PCI-E bus from from hanging by disabling PCI-E master
	 * access and verify no pending requests before reset
	 */
	ixgbe_disable_pcie_master(hw);

mac_reset_top:
	/*
	 * Issue global reset to the MAC.  This needs to be a SW reset.
	 * If link reset is used, it might reset the MAC when mng is using it
	 */
	ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, (ctrl | IXGBE_CTRL_RST));
	IXGBE_WRITE_FLUSH(hw);

	/* Poll for reset bit to self-clear indicating reset is complete */
	for (i = 0; i < 10; i++) {
		usec_delay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST))
			break;
	}
	if (ctrl & IXGBE_CTRL_RST) {
		status = IXGBE_ERR_RESET_FAILED;
		DEBUGOUT("Reset polling failed to complete.\n");
	}

	msec_delay(50);

	/*
	 * Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to allow time
	 * for any pending HW events to complete.
	 */
	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	/*
	 * Store the original AUTOC/AUTOC2 values if they have not been
	 * stored off yet.  Otherwise restore the stored original
	 * values since the reset operation sets back to defaults.
	 */
	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	if (hw->mac.orig_link_settings_stored == FALSE) {
		hw->mac.orig_autoc = autoc;
		hw->mac.orig_autoc2 = autoc2;
		hw->mac.orig_link_settings_stored = TRUE;
	} else {
		if (autoc != hw->mac.orig_autoc)
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC, (hw->mac.orig_autoc |
					IXGBE_AUTOC_AN_RESTART));

		if ((autoc2 & IXGBE_AUTOC2_UPPER_MASK) !=
		    (hw->mac.orig_autoc2 & IXGBE_AUTOC2_UPPER_MASK)) {
			autoc2 &= ~IXGBE_AUTOC2_UPPER_MASK;
			autoc2 |= (hw->mac.orig_autoc2 &
				   IXGBE_AUTOC2_UPPER_MASK);
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC2, autoc2);
		}
	}

	/* Store the permanent mac address */
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.  Also reset num_rar_entries to 128,
	 * since we modify this value when programming the SAN MAC address.
	 */
	hw->mac.num_rar_entries = 128;
	hw->mac.ops.init_rx_addrs(hw);

#if 0
	/* Store the permanent SAN mac address */
	hw->mac.ops.get_san_mac_addr(hw, hw->mac.san_addr);

	/* Add the SAN MAC address to the RAR only if it's a valid address */
	if (ixgbe_validate_mac_addr(hw->mac.san_addr) == 0) {
		hw->mac.ops.set_rar(hw, hw->mac.num_rar_entries - 1,
				    hw->mac.san_addr, 0, IXGBE_RAH_AV);

		/* Save the SAN MAC RAR index */
		hw->mac.san_mac_rar_index = hw->mac.num_rar_entries - 1;

		/* Reserve the last RAR for the SAN MAC address */
		hw->mac.num_rar_entries--;
	}

	/* Store the alternative WWNN/WWPN prefix */
	hw->mac.ops.get_wwn_prefix(hw, &hw->mac.wwnn_prefix,
				       &hw->mac.wwpn_prefix);
#endif
reset_hw_out:
	return status;
}

/**
 *  ixgbe_read_analog_reg8_82599 - Reads 8 bit Omer analog register
 *  @hw: pointer to hardware structure
 *  @reg: analog register to read
 *  @val: read value
 *
 *  Performs read operation to Omer analog register specified.
 **/
int32_t ixgbe_read_analog_reg8_82599(struct ixgbe_hw *hw, uint32_t reg, uint8_t *val)
{
	uint32_t  core_ctl;

	DEBUGFUNC("ixgbe_read_analog_reg8_82599");

	IXGBE_WRITE_REG(hw, IXGBE_CORECTL, IXGBE_CORECTL_WRITE_CMD |
			(reg << 8));
	IXGBE_WRITE_FLUSH(hw);
	usec_delay(10);
	core_ctl = IXGBE_READ_REG(hw, IXGBE_CORECTL);
	*val = (uint8_t)core_ctl;

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_write_analog_reg8_82599 - Writes 8 bit Omer analog register
 *  @hw: pointer to hardware structure
 *  @reg: atlas register to write
 *  @val: value to write
 *
 *  Performs write operation to Omer analog register specified.
 **/
int32_t ixgbe_write_analog_reg8_82599(struct ixgbe_hw *hw, uint32_t reg, uint8_t val)
{
	uint32_t  core_ctl;

	DEBUGFUNC("ixgbe_write_analog_reg8_82599");

	core_ctl = (reg << 8) | val;
	IXGBE_WRITE_REG(hw, IXGBE_CORECTL, core_ctl);
	IXGBE_WRITE_FLUSH(hw);
	usec_delay(10);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_start_hw_82599 - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware using the generic start_hw function
 *  and the generation start_hw function.
 *  Then performs revision-specific operations, if any.
 **/
int32_t ixgbe_start_hw_82599(struct ixgbe_hw *hw)
{
	int32_t ret_val = IXGBE_SUCCESS;
	uint32_t gcr = IXGBE_READ_REG(hw, IXGBE_GCR);

	DEBUGFUNC("ixgbe_start_hw_rev_1__82599");

	ret_val = ixgbe_start_hw_generic(hw);
	if (ret_val != IXGBE_SUCCESS)
		goto out;

	ret_val = ixgbe_start_hw_gen2(hw);
	if (ret_val != IXGBE_SUCCESS)
		goto out;

	/* We need to run link autotry after the driver loads */
	hw->mac.autotry_restart = TRUE;

	/*
	 * From the 82599 specification update:
	 * set the completion timeout value for 16ms to 55ms if needed
	 */
	if (gcr & IXGBE_GCR_CAP_VER2) {
		uint16_t reg;
		reg = IXGBE_READ_PCIE_WORD(hw, IXGBE_PCI_DEVICE_CONTROL2);
		if ((reg & 0x0f) == 0) {
			reg |= IXGBE_PCI_DEVICE_CONTROL2_16ms;
			IXGBE_WRITE_PCIE_WORD(hw, IXGBE_PCI_DEVICE_CONTROL2,
			    reg);
		}
	}

	if (ret_val == IXGBE_SUCCESS)
		ret_val = ixgbe_verify_fw_version_82599(hw);
out:
	return ret_val;
}

/**
 *  ixgbe_identify_phy_82599 - Get physical layer module
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 *  If PHY already detected, maintains current PHY type in hw struct,
 *  otherwise executes the PHY detection routine.
 **/
int32_t ixgbe_identify_phy_82599(struct ixgbe_hw *hw)
{
	int32_t status = IXGBE_ERR_PHY_ADDR_INVALID;

	DEBUGFUNC("ixgbe_identify_phy_82599");

	/* Detect PHY if not unknown - returns success if already detected. */
	status = ixgbe_identify_phy_generic(hw);
	if (status != IXGBE_SUCCESS) {
		/* 82599 10GBASE-T requires an external PHY */
		if (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_copper)
			goto out;
		else
			status = ixgbe_identify_sfp_module_generic(hw);
	}

	/* Set PHY type none if no PHY detected */
	if (hw->phy.type == ixgbe_phy_unknown) {
		hw->phy.type = ixgbe_phy_none;
		status = IXGBE_SUCCESS;
	}

	/* Return error if SFP module has been detected but is not supported */
	if (hw->phy.type == ixgbe_phy_sfp_unsupported)
		status = IXGBE_ERR_SFP_NOT_SUPPORTED;

out:
	return status;
}

/**
 *  ixgbe_get_supported_physical_layer_82599 - Returns physical layer type
 *  @hw: pointer to hardware structure
 *
 *  Determines physical layer capabilities of the current configuration.
 **/
uint32_t ixgbe_get_supported_physical_layer_82599(struct ixgbe_hw *hw)
{
	uint32_t physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	uint32_t autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	uint32_t autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	uint32_t pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	uint32_t pma_pmd_10g_parallel = autoc & IXGBE_AUTOC_10G_PMA_PMD_MASK;
	uint32_t pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;
	uint16_t ext_ability = 0;
	uint8_t comp_codes_10g = 0;
	uint8_t comp_codes_1g = 0;

	DEBUGFUNC("ixgbe_get_support_physical_layer_82599");

	hw->phy.ops.identify(hw);

	switch (hw->phy.type) {
	case ixgbe_phy_tn:
	case ixgbe_phy_cu_unknown:
		hw->phy.ops.read_reg(hw, IXGBE_MDIO_PHY_EXT_ABILITY,
		IXGBE_MDIO_PMA_PMD_DEV_TYPE, &ext_ability);
		if (ext_ability & IXGBE_MDIO_PHY_10GBASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
		if (ext_ability & IXGBE_MDIO_PHY_1000BASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
		if (ext_ability & IXGBE_MDIO_PHY_100BASETX_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_100BASE_TX;
		goto out;
	default:
		break;
	}

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_AN:
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		if (pma_pmd_1g == IXGBE_AUTOC_1G_KX_BX) {
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_KX |
			    IXGBE_PHYSICAL_LAYER_1000BASE_BX;
			goto out;
		} else
			/* SFI mode so read SFP module */
			goto sfp_check;
		break;
	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_CX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_CX4;
		else if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_KX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		else if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_XAUI)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_XAUI;
		goto out;
		break;
	case IXGBE_AUTOC_LMS_10G_SERIAL:
		if (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_KR) {
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KR;
			goto out;
		} else if (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI)
			goto sfp_check;
		break;
	case IXGBE_AUTOC_LMS_KX4_KX_KR:
	case IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KR;
		goto out;
		break;
	default:
		goto out;
		break;
	}

sfp_check:
	/* SFP check must be done last since DA modules are sometimes used to
	 * test KR mode -  we need to id KR mode correctly before SFP module.
	 * Call identify_sfp because the pluggable module may have changed */
	hw->phy.ops.identify_sfp(hw);
	if (hw->phy.sfp_type == ixgbe_sfp_type_not_present)
		goto out;

	switch (hw->phy.type) {
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
		physical_layer = IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU;
		break;
	case ixgbe_phy_sfp_ftl_active:
	case ixgbe_phy_sfp_active_unknown:
		physical_layer = IXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA;
		break;
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
		hw->phy.ops.read_i2c_eeprom(hw,
		      IXGBE_SFF_1GBE_COMP_CODES, &comp_codes_1g);
		hw->phy.ops.read_i2c_eeprom(hw,
		      IXGBE_SFF_10GBE_COMP_CODES, &comp_codes_10g);
		if (comp_codes_10g & IXGBE_SFF_10GBASESR_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_SR;
		else if (comp_codes_10g & IXGBE_SFF_10GBASELR_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_LR;
		else if (comp_codes_10g &
		    (IXGBE_SFF_DA_PASSIVE_CABLE | IXGBE_SFF_DA_BAD_HP_CABLE))
			physical_layer = IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU;
		else if (comp_codes_10g & IXGBE_SFF_DA_ACTIVE_CABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA;
		else if (comp_codes_1g & IXGBE_SFF_1GBASET_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_T;
		break;
	default:
		break;
	}

out:
	return physical_layer;
}

/**
 *  ixgbe_enable_rx_dma_82599 - Enable the Rx DMA unit on 82599
 *  @hw: pointer to hardware structure
 *  @regval: register value to write to RXCTRL
 *
 *  Enables the Rx DMA unit for 82599
 **/
int32_t ixgbe_enable_rx_dma_82599(struct ixgbe_hw *hw, uint32_t regval)
{
#define IXGBE_MAX_SECRX_POLL 30
	int i;
	int secrxreg;

	DEBUGFUNC("ixgbe_enable_rx_dma_82599");

	/*
	 * Workaround for 82599 silicon errata when enabling the Rx datapath.
	 * If traffic is incoming before we enable the Rx unit, it could hang
	 * the Rx DMA unit.  Therefore, make sure the security engine is
	 * completely disabled prior to enabling the Rx unit.
	 */
	secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	secrxreg |= IXGBE_SECRXCTRL_RX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, secrxreg);
	for (i = 0; i < IXGBE_MAX_SECRX_POLL; i++) {
		secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXSTAT);
		if (secrxreg & IXGBE_SECRXSTAT_SECRX_RDY)
			break;
		else
			/* Use interrupt-safe sleep just in case */
			usec_delay(1000);
	}

	/* For informational purposes only */
	if (i >= IXGBE_MAX_SECRX_POLL)
		DEBUGOUT("Rx unit being enabled before security "
			 "path fully disabled.  Continuing with init.\n");

	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, regval);
	secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	secrxreg &= ~IXGBE_SECRXCTRL_RX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, secrxreg);
	IXGBE_WRITE_FLUSH(hw);

	return IXGBE_SUCCESS;
}

/**
 *  ixgbe_verify_fw_version_82599 - verify fw version for 82599
 *  @hw: pointer to hardware structure
 *
 *  Verifies that installed the firmware version is 0.6 or higher
 *  for SFI devices. All 82599 SFI devices should have version 0.6 or higher.
 *
 *  Returns IXGBE_ERR_EEPROM_VERSION if the FW is not present or
 *  if the FW version is not supported.
 **/
int32_t ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw)
{
	int32_t status = IXGBE_ERR_EEPROM_VERSION;
	uint16_t fw_offset, fw_ptp_cfg_offset;
	uint16_t fw_version = 0;

	DEBUGFUNC("ixgbe_verify_fw_version_82599");

	/* firmware check is only necessary for SFI devices */
	if (hw->phy.media_type != ixgbe_media_type_fiber) {
		status = IXGBE_SUCCESS;
		goto fw_version_out;
	}

	/* get the offset to the Firmware Module block */
	hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset);

	if ((fw_offset == 0) || (fw_offset == 0xFFFF))
		goto fw_version_out;

	/* get the offset to the Pass Through Patch Configuration block */
	hw->eeprom.ops.read(hw, (fw_offset +
				 IXGBE_FW_PASSTHROUGH_PATCH_CONFIG_PTR),
				 &fw_ptp_cfg_offset);

	if ((fw_ptp_cfg_offset == 0) || (fw_ptp_cfg_offset == 0xFFFF))
		goto fw_version_out;

	/* get the firmware version */
	hw->eeprom.ops.read(hw, (fw_ptp_cfg_offset +
				 IXGBE_FW_PATCH_VERSION_4),
				 &fw_version);

	if (fw_version > 0x5)
		status = IXGBE_SUCCESS;

fw_version_out:
	return status;
}

/**
 *  ixgbe_verify_lesm_fw_enabled_82599 - Checks LESM FW module state.
 *  @hw: pointer to hardware structure
 *
 *  Returns TRUE if the LESM FW module is present and enabled. Otherwise
 *  returns FALSE. Smart Speed must be disabled if LESM FW module is enabled.
 **/
int ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw)
{
	int lesm_enabled = FALSE;
	uint16_t fw_offset, fw_lesm_param_offset, fw_lesm_state;
	int32_t status;

	DEBUGFUNC("ixgbe_verify_lesm_fw_enabled_82599");

	/* get the offset to the Firmware Module block */
	status = hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset);

	if ((status != IXGBE_SUCCESS) ||
	    (fw_offset == 0) || (fw_offset == 0xFFFF))
		goto out;

	/* get the offset to the LESM Parameters block */
	status = hw->eeprom.ops.read(hw, (fw_offset +
				 IXGBE_FW_LESM_PARAMETERS_PTR),
				 &fw_lesm_param_offset);

	if ((status != IXGBE_SUCCESS) ||
	    (fw_lesm_param_offset == 0) || (fw_lesm_param_offset == 0xFFFF))
		goto out;

	/* get the lesm state word */
	status = hw->eeprom.ops.read(hw, (fw_lesm_param_offset +
				     IXGBE_FW_LESM_STATE_1),
				     &fw_lesm_state);

	if ((status == IXGBE_SUCCESS) &&
	    (fw_lesm_state & IXGBE_FW_LESM_STATE_ENABLED))
		lesm_enabled = TRUE;

out:
	return lesm_enabled;
}
