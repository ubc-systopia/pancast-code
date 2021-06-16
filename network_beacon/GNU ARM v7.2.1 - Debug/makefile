################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_protocol_crypto/src/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_alt/source/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/subdir.mk
-include gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/subdir.mk
-include gecko_sdk_3.1.1/util/silicon_labs/silabs_core/memory_manager/subdir.mk
-include gecko_sdk_3.1.1/platform/service/udelay/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/system/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/sleeptimer/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/ram_interrupt_vector_init/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/power_manager/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/iostream/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/hfxo_manager/src/subdir.mk
-include gecko_sdk_3.1.1/platform/service/device_init/src/subdir.mk
-include gecko_sdk_3.1.1/platform/radio/rail_lib/plugin/pa-conversions/subdir.mk
-include gecko_sdk_3.1.1/platform/emlib/src/subdir.mk
-include gecko_sdk_3.1.1/platform/emdrv/nvm3/src/subdir.mk
-include gecko_sdk_3.1.1/platform/common/toolchain/src/subdir.mk
-include gecko_sdk_3.1.1/platform/common/src/subdir.mk
-include gecko_sdk_3.1.1/platform/Device/SiliconLabs/EFR32MG22/Source/GCC/subdir.mk
-include gecko_sdk_3.1.1/platform/Device/SiliconLabs/EFR32MG22/Source/subdir.mk
-include gecko_sdk_3.1.1/hardware/driver/mx25_flash_shutdown/src/sl_mx25_flash_shutdown_usart/subdir.mk
-include gecko_sdk_3.1.1/hardware/board/src/subdir.mk
-include autogen/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: network_beacon.axf

# Tool invocations
network_beacon.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU ARM C Linker'
	arm-none-eabi-gcc -g3 -gdwarf-2 -mcpu=cortex-m33 -mthumb -T "/Users/nboufford/workspace/pancast/pancast-code/network_beacon/autogen/linkerfile.ld" -Xlinker --gc-sections -Xlinker -Map="network_beacon.map" -mfpu=fpv5-sp-d16 -mfloat-abi=hard --specs=nano.specs -o network_beacon.axf -Wl,--start-group "./app.o" "./app_iostream_eusart.o" "./main.o" "./autogen/sl_bluetooth.o" "./autogen/sl_board_default_init.o" "./autogen/sl_device_init_clocks.o" "./autogen/sl_event_handler.o" "./autogen/sl_iostream_handles.o" "./autogen/sl_iostream_init_eusart_instances.o" "./autogen/sl_power_manager_handler.o" "./gecko_sdk_3.1.1/hardware/board/src/sl_board_control_gpio.o" "./gecko_sdk_3.1.1/hardware/board/src/sl_board_init.o" "./gecko_sdk_3.1.1/hardware/driver/mx25_flash_shutdown/src/sl_mx25_flash_shutdown_usart/sl_mx25_flash_shutdown.o" "./gecko_sdk_3.1.1/platform/Device/SiliconLabs/EFR32MG22/Source/GCC/startup_efr32mg22.o" "./gecko_sdk_3.1.1/platform/Device/SiliconLabs/EFR32MG22/Source/system_efr32mg22.o" "./gecko_sdk_3.1.1/platform/common/src/sl_slist.o" "./gecko_sdk_3.1.1/platform/common/toolchain/src/sl_memory.o" "./gecko_sdk_3.1.1/platform/emdrv/nvm3/src/nvm3_default_common_linker.o" "./gecko_sdk_3.1.1/platform/emdrv/nvm3/src/nvm3_hal_flash.o" "./gecko_sdk_3.1.1/platform/emdrv/nvm3/src/nvm3_lock.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_assert.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_burtc.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_cmu.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_core.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_emu.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_eusart.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_gpio.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_msc.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_rtcc.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_se.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_system.o" "./gecko_sdk_3.1.1/platform/emlib/src/em_usart.o" "./gecko_sdk_3.1.1/platform/radio/rail_lib/plugin/pa-conversions/pa_conversions_efr32.o" "./gecko_sdk_3.1.1/platform/radio/rail_lib/plugin/pa-conversions/pa_curves_efr32.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_dcdc_s2.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_emu_s2.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_hfxo_s2.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_lfrco.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_lfxo_s2.o" "./gecko_sdk_3.1.1/platform/service/device_init/src/sl_device_init_nvic.o" "./gecko_sdk_3.1.1/platform/service/hfxo_manager/src/sl_hfxo_manager.o" "./gecko_sdk_3.1.1/platform/service/hfxo_manager/src/sl_hfxo_manager_hal_s2.o" "./gecko_sdk_3.1.1/platform/service/iostream/src/sl_iostream.o" "./gecko_sdk_3.1.1/platform/service/iostream/src/sl_iostream_eusart.o" "./gecko_sdk_3.1.1/platform/service/iostream/src/sl_iostream_retarget_stdio.o" "./gecko_sdk_3.1.1/platform/service/iostream/src/sl_iostream_uart.o" "./gecko_sdk_3.1.1/platform/service/power_manager/src/sl_power_manager.o" "./gecko_sdk_3.1.1/platform/service/power_manager/src/sl_power_manager_debug.o" "./gecko_sdk_3.1.1/platform/service/power_manager/src/sl_power_manager_hal_s2.o" "./gecko_sdk_3.1.1/platform/service/ram_interrupt_vector_init/src/sl_ram_interrupt_vector_init.o" "./gecko_sdk_3.1.1/platform/service/sleeptimer/src/sl_sleeptimer.o" "./gecko_sdk_3.1.1/platform/service/sleeptimer/src/sl_sleeptimer_hal_burtc.o" "./gecko_sdk_3.1.1/platform/service/sleeptimer/src/sl_sleeptimer_hal_prortc.o" "./gecko_sdk_3.1.1/platform/service/sleeptimer/src/sl_sleeptimer_hal_rtcc.o" "./gecko_sdk_3.1.1/platform/service/system/src/sl_system_init.o" "./gecko_sdk_3.1.1/platform/service/system/src/sl_system_process_action.o" "./gecko_sdk_3.1.1/platform/service/udelay/src/sl_udelay.o" "./gecko_sdk_3.1.1/platform/service/udelay/src/sl_udelay_armv6m_gcc.o" "./gecko_sdk_3.1.1/util/silicon_labs/silabs_core/memory_manager/sl_malloc.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/aes.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/cipher.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/cipher_wrap.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/ctr_drbg.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/entropy.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/error.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/hmac_drbg.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/platform.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/platform_util.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/psa_crypto.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/psa_crypto_driver_wrappers.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/psa_crypto_slot_management.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/psa_crypto_storage.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/sha256.o" "./gecko_sdk_3.1.1/util/third_party/crypto/mbedtls/library/threading.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_attestation.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_cipher.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_entropy.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_hash.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_key_derivation.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_key_handling.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_signature.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/se_manager/src/sl_se_manager_util.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_alt/source/sl_entropy.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_alt/source/sl_mbedtls.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_alt/source/sl_psa_its_nvm3.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/ba414ep_config.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/ba431_config.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/cryptodma_internal.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/cryptolib_types.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_aes.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_blk_cipher.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_dh_alg.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_ecc_curves.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_ecc_keygen_alg.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_ecdsa_alg.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_hash.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_math.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_memcmp.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_memcpy.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_primitives.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_rng.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_cryptoacc_library/src/sx_trng.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/cryptoacc_aes.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/cryptoacc_ccm.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/cryptoacc_gcm.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/mbedtls_cmac.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/mbedtls_ecdsa_ecdh.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_mbedtls_support/src/mbedtls_sha.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_protocol_crypto/src/sli_protocol_crypto_radioaes.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_protocol_crypto/src/sli_radioaes_management.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/cryptoacc_management.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_aead.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_cipher.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_hash.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_key_derivation.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_key_management.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_mac.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_cryptoacc_transparent_driver_signature.o" "./gecko_sdk_3.1.1/util/third_party/crypto/sl_component/sl_psa_driver/src/sli_psa_driver_init.o" "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/sdks/gecko_sdk_suite/v3.1//platform/emdrv/nvm3/lib/libnvm3_CM33_gcc.a" "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/sdks/gecko_sdk_suite/v3.1//protocol/bluetooth/lib/EFR32MG22/GCC/libbluetooth.a" "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/sdks/gecko_sdk_suite/v3.1//platform/radio/rail_lib/autogen/librail_release/librail_efr32xg22_gcc_release.a" -lgcc -lc -lm -lnosys -Wl,--end-group -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

	@echo 'Building hex file: network_beacon.hex'
	arm-none-eabi-objcopy -O ihex "network_beacon.axf" "network_beacon.hex"
	@echo ' '

	@echo 'Building bin file: network_beacon.bin'
	arm-none-eabi-objcopy -O binary "network_beacon.axf" "network_beacon.bin"
	@echo ' '

	@echo 'Building s37 file: network_beacon.s37'
	arm-none-eabi-objcopy -O srec "network_beacon.axf" "network_beacon.s37"
	@echo ' '

	@echo 'Running size tool'
	arm-none-eabi-size "network_beacon.axf" -A
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) network_beacon.axf
	-@echo ' '

.PHONY: all clean dependents

-include ../makefile.targets
